#include <assert.h>
#include <iostream>
#include <set>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

#include "VulkanRenderer.h"
#include "Utilities.h"

// This is the list of validation layers we want
const std::vector<const char*> VALIDATION_LAYERS = {
	"VK_LAYER_LUNARG_standard_validation"
};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallbackFn(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData)
{
	std::cerr << pMessage << std::endl;
	return VK_FALSE;
}

VkResult 
CreateDebugReportCallbackEXT(
	VkInstance vkInstance,
	const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugReportCallbackEXT* pCallback
	) 
{
	auto func = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(vkInstance, "vkCreateDebugReportCallbackEXT"));
	if (func != nullptr) {
		return func(vkInstance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void
DestroyDebugReportCallbackEXT(
    VkInstance vkInstance,
    VkDebugReportCallbackEXT pCallback,
    const VkAllocationCallbacks* pAllocator
    )
{
    auto func = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugReportCallbackEXT"));
    if (func != nullptr)
    {
        func(vkInstance, pCallback, pAllocator);
    }
}

// ------------------

VulkanRenderer::VulkanRenderer(
	GLFWwindow* window,
	Scene* scene
	)
	:
	Renderer(window, scene)
	, m_instance(VK_NULL_HANDLE) 
	, m_debugCallback(VK_NULL_HANDLE)
	, m_surfaceKHR(VK_NULL_HANDLE)
	, m_physicalDevice(VK_NULL_HANDLE)
	, m_device(VK_NULL_HANDLE)
	, m_graphicsQueue(VK_NULL_HANDLE)
	, m_presentQueue(VK_NULL_HANDLE)
	, m_name("Vulkan Application")
{
    // -- Initialize logger

    // Combine console and file logger
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_st>());
    // Create a 5MB rotating logger
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_st>("VulkanRenderer", "log", 1024 * 1024 * 5, 3));
    m_logger = std::make_shared<spdlog::logger>("Logger", begin(sinks), end(sinks));
    m_logger->set_pattern("<%H:%M:%S>[%I] %v");

    // -- Initialize Vulkan

	VkResult result = CreateInstance();
	assert(result == VK_SUCCESS);
    m_logger->info<std::string>("Initalizes Vulkan instance");

// Only enable the validation layer when running in debug mode
#ifdef NDEBUG
	m_isEnableValidationLayers = false;
#else
	m_isEnableValidationLayers = true;
#endif

	result = SetupDebugCallback();
	assert(result == VK_SUCCESS);
    m_logger->info<std::string>("Setup debug callback");

	result = CreateWindowSurface();
	assert(result == VK_SUCCESS);
    m_logger->info<std::string>("Created window surface");

	result = SelectPhysicalDevice();
	assert(result == VK_SUCCESS);
    m_logger->info<std::string>("Selected physical device");

	result = SetupLogicalDevice();
	assert(result == VK_SUCCESS);
    m_logger->info<std::string>("Setup logical device");

	result = CreateSwapchain();
	assert(result == VK_SUCCESS);
    m_logger->info<std::string>("Created swapchain");

    result = CreateImageViews();
    assert(result == VK_SUCCESS);
    m_logger->info("Created {} VkImageViews", m_swapchain.imageViews.size());

	result = CreateRenderPass();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created renderpass");

	result = CreateDescriptorSetLayout();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created descriptor set layout");

	result = CreateGraphicsPipeline();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created graphics pipeline");

	result = CreateCommandPool();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created command pool");

	result = CreateDepthResources();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created depth image");

	result = CreateFramebuffers();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created framebuffers");

	result = CreateVertexBuffer();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created vertex buffer");

	result = CreateUniformBuffer();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created uniform buffer");

	result = CreateDescriptorPool();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created descriptor pool");

	result = CreateDescriptorSet();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created descriptor set");

	result = CreateCommandBuffers();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created command buffers");

	result = CreateSemaphores();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created semaphores");

}


VulkanRenderer::~VulkanRenderer()
{
	// Free memory in the opposite order of creation

	vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
	
	vkFreeCommandBuffers(m_device, m_graphicsCommandPool, m_commandBuffers.size(), m_commandBuffers.data());
	
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyImageView(m_device, m_depthImageView, nullptr);
	vkDestroyImage(m_device, m_depthImage, nullptr);
	vkFreeMemory(m_device, m_depthImageMemory, nullptr);

	for (GeometryBuffer& geomBuffer : m_geometryBuffers) {
		vkFreeMemory(m_device, geomBuffer.vertexBufferMemory, nullptr);
		vkDestroyBuffer(m_device, geomBuffer.vertexBuffer, nullptr);
	}

	vkFreeMemory(m_device, m_uniformStagingBufferMemory, nullptr);
	vkDestroyBuffer(m_device, m_uniformStagingBuffer, nullptr);
	vkFreeMemory(m_device, m_uniformBufferMemory, nullptr);
	vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
	
	vkDestroyCommandPool(m_device, m_graphicsCommandPool, nullptr);
	for (auto& frameBuffer : m_swapchain.framebuffers) {
		vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
	}
	
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);
	
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
	
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
	for (auto& imageView : m_swapchain.imageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	
	vkDestroySwapchainKHR(m_device, m_swapchain.swapchain, nullptr);
	
	vkDestroyDevice(m_device, nullptr);
	
	vkDestroySurfaceKHR(m_instance, m_surfaceKHR, nullptr);
    
	DestroyDebugReportCallbackEXT(m_instance, m_debugCallback, nullptr);
	
	vkDestroyInstance(m_instance, nullptr);
}

VkResult 
VulkanRenderer::CreateInstance() 
{	
	// Create application info struct
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = m_name.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = m_name.c_str();
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	// Grab extensions. This includes the KHR surface extension and debug layer if in debug mode
	std::vector<const char*> extensions = GetInstanceRequiredExtensions(m_isEnableValidationLayers);

	// Create instance info struct
	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	// Grab validation layers
	if (m_isEnableValidationLayers) {
		assert(CheckValidationLayerSupport(VALIDATION_LAYERS));
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		instanceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	} else {
		instanceCreateInfo.enabledLayerCount = 0;
	}

	return vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
}

VkResult 
VulkanRenderer::SetupDebugCallback() 
{
	
	// Do nothing if we have not enabled debug mode
	if (!m_isEnableValidationLayers) {
		return VK_SUCCESS;
	}

	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {};
	debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	// Report errors and warnings back
	debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	debugReportCallbackCreateInfo.pfnCallback = DebugCallbackFn;

	return CreateDebugReportCallbackEXT(m_instance, &debugReportCallbackCreateInfo, nullptr, &m_debugCallback);
}

VkResult 
VulkanRenderer::CreateWindowSurface() 
{
	VkResult result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surfaceKHR);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface");
	}

	return result;
}


VkResult 
VulkanRenderer::SelectPhysicalDevice() 
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
	
	if (physicalDeviceCount == 0) {
		throw std::runtime_error("Failed to find a GPU that supports Vulkan");
	}

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());
	
	for (auto physicalDevice : physicalDevices) {
		if (IsDeviceVulkanCompatible(physicalDevice, m_surfaceKHR)) {
			m_physicalDevice = physicalDevice;
			break;
		}
	}

	return m_physicalDevice != nullptr ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
}

VkResult 
VulkanRenderer::SetupLogicalDevice() 
{
	// @todo: should add new & interesting features later
	VkPhysicalDeviceFeatures deviceFeatures = {};

	// Create logical device info struct and populate it
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	// Create a set of unique queue families for the required queues
	m_queueFamilyIndices = FindQueueFamilyIndices(m_physicalDevice, m_surfaceKHR);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	set<int> uniqueQueueFamilies = { m_queueFamilyIndices.graphicsFamily, m_queueFamilyIndices.presentFamily };
	for (auto i : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily;
		queueCreateInfo.queueCount = 1; // Only using one queue for now. We actually don't need that many.
		float queuePriority = 1.0f; // Queue priority. Required even if we only have one queue.
		queueCreateInfo.pQueuePriorities = &queuePriority;

		// Append to queue create infos
		queueCreateInfos.push_back(queueCreateInfo);
	}

	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

	// Grab logical device extensions
	std::vector<const char*> enabledExtensions = GetDeviceRequiredExtensions(m_physicalDevice);
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

	// Grab validation layers
	if (m_isEnableValidationLayers) {
		assert(CheckValidationLayerSupport(VALIDATION_LAYERS));
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		deviceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	}
	else {
		deviceCreateInfo.enabledLayerCount = 0;
	}

	// Create the logical device
	VkResult result = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
	
	// Grabs the first queue in the graphics queue family since we only need one graphics queue anyway
	vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);

	// Grabs the first queue in the present queue family since we only need one present queue anyway
	vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
	
	return result;
}

VkResult
VulkanRenderer::CreateSwapchain()
{
	// Query existing support
	SwapchainSupport swapchainSupport = QuerySwapchainSupport(m_physicalDevice, m_surfaceKHR);

	// Select three settings:
	// 1. Surface format
	// 2. Present mode
	// 3. Extent
	// @todo Right now the preference is embedded inside these functions. We need to expose it to a global configuration file somewhere instead.
	VkSurfaceFormatKHR surfaceFormat = SelectDesiredSwapchainSurfaceFormat(swapchainSupport.surfaceFormats);
	VkPresentModeKHR presentMode = SelectDesiredSwapchainPresentMode(swapchainSupport.presentModes);
	VkExtent2D extent = SelectDesiredSwapchainExtent(swapchainSupport.capabilities);

	// Select the number of images to be in our swapchain queue. To properly implement tripple buffering, 
	// we might need an extra image in the queue so bumb the count up. Also, create info struct
	// for swap chain requires the minimum image count
	uint32_t minImageCount = swapchainSupport.capabilities.minImageCount + 1;

	// Vulkan also specifies that a the maxImageCount could be 0 to indicate that there is no limit besides memory requirement.
	if (swapchainSupport.capabilities.maxImageCount > 0 &&
		minImageCount > swapchainSupport.capabilities.maxImageCount)
	{
		// Just need to restrict it to be something greater than the capable minImageCount but not greater than
		// maxImageCount if we're limited on maxImageCount
		minImageCount = swapchainSupport.capabilities.maxImageCount;
	}

	// Construct the swapchain create info struct
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = m_surfaceKHR;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.imageExtent = extent;

	// Amount of layers for each image. This is always 1 unless we're developing for stereoscopic 3D app.
	swapchainCreateInfo.imageArrayLayers = 1;

	// See ref. for a list of different image usage flags we could use
	// @ref: https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkImageUsageFlagBits
	// @todo: For now, we're just rendering directly to the image view as color attachment. It's possible to use
	// something like VK_IMAGE_USAGE_TRANSFER_DST_BIT to transfer to another image for post-processing before presenting.
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Swap chain is set up differently depending on whether or not 
	// our graphics queue and present queue are from the same family
	// Images will be drawn on the graphics queue then submitted to the present queue.
	assert(m_queueFamilyIndices.IsComplete());
	if (m_queueFamilyIndices.presentFamily == m_queueFamilyIndices.graphicsFamily) 
	{
		// Image is owned by a single queue family. Best performance. 
		// This is the case on most hardware.
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0; // Optional
		swapchainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
	} 
	else 
	{
		// Image can be used by multiple queue families.
		const uint32_t indices[2] = {
			static_cast<uint32_t>(m_queueFamilyIndices.graphicsFamily),
			static_cast<uint32_t>(m_queueFamilyIndices.presentFamily),
		};
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = indices;
	}

	// This transform is applied to the image in the swapchain.
	// For ex. a 90 rotation. We don't want any transformation here for now.
	swapchainCreateInfo.preTransform = swapchainSupport.capabilities.currentTransform;

	// This is the blending to be applied with other windows
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	// Don't care about pixels obscured, unless we need them to compute some prediction.
	swapchainCreateInfo.clipped = VK_TRUE;

	// This is handle for a backup swapchain in case our current one is trashed and
	// we need to recover. 
	// @todo NULL for now.
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create the swap chain now!
	VkResult result = vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain.swapchain);
	if (result != VK_SUCCESS) 
	{
        throw std::runtime_error("Failed to create swapchain!");
	}

    // Initialize the vector of swapchain images here. 
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(m_device, m_swapchain.swapchain, &imageCount, nullptr);
    m_swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain.swapchain, &imageCount, m_swapchain.images.data());

    // Initialize other swapchain related fields
    m_swapchain.imageFormat = surfaceFormat.format;
    m_swapchain.extent = extent;

	return result;
}

VkResult 
VulkanRenderer::CreateImageViews() 
{
    VkResult result = VK_SUCCESS;

    m_swapchain.imageViews.resize(m_swapchain.images.size());
    for (auto i = 0; i < m_swapchain.imageViews.size(); ++i) {
		CreateImageView(
			m_swapchain.images[i],
			VK_IMAGE_VIEW_TYPE_2D,
			m_swapchain.imageFormat,
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_swapchain.imageViews[i]
		);
    }
    return result;
}

VkResult 
VulkanRenderer::CreateRenderPass() 
{
	// ----- Specify color attachment ------

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_swapchain.imageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// What to do with the color attachment before loading rendered content
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear to black
	// What to do after rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Read back rendered image
	// Stencil ops
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// Pixel layout of VkImage objects in memory
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // To be presented in swapchain

	// Create subpasses. A phase of rendering within a render pass, that reads and writes a subset of the attachments
	// Subpass can depend on previous rendered framebuffers, so it could be used for post-processing.
	// It uses the attachment reference of the color attachment specified above.
	VkAttachmentReference colorAttachmentRef = {};
	// Index to which attachment in the attachment descriptions array. This is equivalent to the attachment at layout(location = 0)
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// ------ Depth attachment -------

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = FindDepthFormat(m_physicalDevice);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Store if we're going to do deferred rendering
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// ------ Subpass -------

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Make this subpass binds to graphics point
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	// Subpass dependency
	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	// Wait until the swapchain finishes before reading it
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	// Read and write to color attachment
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// ------ Renderpass creation -------

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = attachments.size();
	renderPassCreateInfo.pAttachments = attachments.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;

	VkResult result = vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass");
		return result;
	}

	return result;
}

VkResult 
VulkanRenderer::CreateDescriptorSetLayout() 
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	VkResult result = vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor set layout");
		return result;
	}
	
	return result;
}

VkResult 
VulkanRenderer::CreateGraphicsPipeline() {
	
	VkResult result = VK_SUCCESS;

	// Load SPIR-V bytecode
	// The SPIR_V files can be compiled by running glsllangValidator.exe from the VulkanSDK or
	// by invoking the custom script shaders/compileShaders.bat
	std::vector<char> vertShaderBytecode;
	std::vector<char> fragShaderBytecode;
	LoadSPIR_V("shaders/vert.spv", "shaders/frag.spv", vertShaderBytecode, fragShaderBytecode);
	m_logger->info("Loaded {} vertex shader, file size {} bytes", "shaders/vert.spv", vertShaderBytecode.size());
	m_logger->info("Loaded {} frag shader, file size {} bytes", "shaders/frag.spv", fragShaderBytecode.size());
	
	// Create shader modules from bytecodes
	VkShaderModule vertShader;
	CreateShaderModule(
		vertShaderBytecode,
		vertShader
		);

	VkShaderModule fragShader;
	CreateShaderModule(
		fragShaderBytecode,
		fragShader
	);

	// -- Setup the programmable stages for the pipeline. This links the shader modules with their corresponding stages.
	// \ref https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineShaderStageCreateInfo
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo = {};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = vertShader;
	vertShaderStageCreateInfo.pName = "main"; // Specify entry point. It's possible to combine multiple shaders into a single shader module
	
	// This can be used to set values for shader constants. The compiler can perform optimization for these constants vs. if they're created as variables in the shaders.
	vertShaderStageCreateInfo.pSpecializationInfo = nullptr; 
	
	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = {};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = fragShader;
	fragShaderStageCreateInfo.pName = "main"; // Specify entry point. It's possible to combine multiple shaders into a single shader module

	// This can be used to set values for shader constants. The compiler can perform optimization for these constants vs. if they're created as variables in the shaders.
	fragShaderStageCreateInfo.pSpecializationInfo = nullptr;

	// -- Setup the fixed functions stages for the pipeline.
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineVertexInputStateCreateInfo
	// 1. Vertex input stage
	VkPipelineVertexInputStateCreateInfo vertexInputStageCreateInfo = {};
	vertexInputStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	// Input binding description
	VkVertexInputBindingDescription bindingDesc[2] = {
		GetVertexInputBindingDescription(0, m_scene->m_geometriesData[0]->vertexAttributes.at(POSITION)),
		GetVertexInputBindingDescription(1, m_scene->m_geometriesData[0]->vertexAttributes.at(NORMAL))
	};
	vertexInputStageCreateInfo.vertexBindingDescriptionCount = 2;
	vertexInputStageCreateInfo.pVertexBindingDescriptions = bindingDesc;
	
	// Attribute description (position, normal, texcoord etc.)
	auto vertAttribDesc = GetAttributeDescriptions();
	vertexInputStageCreateInfo.vertexAttributeDescriptionCount = vertAttribDesc.size();
	vertexInputStageCreateInfo.pVertexAttributeDescriptions = vertAttribDesc.data();

	// 2. Input assembly
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineInputAssemblyStateCreateInfo
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE; // If true, we can break up primitives like triangels and lines using a special index 0xFFFF

	// 3. Skip tesselation 
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineTessellationStateCreateInfo


	// 4. Viewports and scissors
	// Viewport typically just covers the entire swapchain extent
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineViewportStateCreateInfo
	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchain.extent.width);
	viewport.height = static_cast<float>(m_swapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = m_swapchain.extent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	// 5. Rasterizer
	// This stage converts primitives into fragments. It also performs depth/stencil testing, face culling, scissor test.
	// Rasterization state can be affected by three things: 
	// - VkPipelineRasterizationStateCreateInfo
	// - VkPipelineMultisampleStateCreateInfo 
	// - VkPipelineDepthStencilStateCreateInfo
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineRasterizationStateCreateInfo 

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	// If enabled, fragments beyond near and far planes are clamped instead of discarded
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE; 
	// If enabled, geometry won't pass through rasterization. This would be useful for transform feedbacks
	// where we don't need to go through the fragment shader
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL; // fill, line, or point
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	// 6. Multisampling state. We're not doing anything special here for now
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineMultisampleStateCreateInfo

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.minSampleShading = 1.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	// 6. Depth/stecil tests state
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineDepthStencilStateCreateInfo
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; // 1.0f is farthest, 0.0f is closest
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE; // Allowing for transparent objects
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE; // Allowing to keep fragment falling withn a  certain range
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {}; // For stencil test

	// 7. Color blending state
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineColorBlendStateCreateInfo
	
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// Do minimal work for now, so no blending. This will be useful for later on when we want to do pixel blending (alpha blending).
	// There are a lot of interesting blending we can do here.
	colorBlendAttachmentState.blendEnable = VK_FALSE; 
	
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	// 8. Dynamic state. Some pipeline states can be updated dynamically. Skip for now.

	// 9. Create pipeline layout to hold uniforms. This can be modified dynamically. 
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	result = vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline layout.");
		return result;
	}

	// Finally, create our graphics pipeline here!
	VkPipelineShaderStageCreateInfo createInfos[2] = { vertShaderStageCreateInfo, fragShaderStageCreateInfo };
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = 2; // Number of shader stages
	graphicsPipelineCreateInfo.pStages = createInfos;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStageCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr; // Skipped
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = nullptr; // Skipped
	graphicsPipelineCreateInfo.layout = m_pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = m_renderPass;
	graphicsPipelineCreateInfo.subpass = 0; // Index to the subpass we'll be using

	// Since pipelins are expensive to create, potentially we could reuse a common parent pipeline.
	// We just have one here so we don't need to specify these values.
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	// We can also cache the pipeline object and store it in a file for resuse
	result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline");
		return result;
	}

	// We don't need the shader modules after the graphics pipeline creation. Destroy them now.
	vkDestroyShaderModule(m_device, vertShader, nullptr);
	vkDestroyShaderModule(m_device, fragShader, nullptr);

	return result;
}

VkResult 
VulkanRenderer::CreateFramebuffers() 
{
	VkResult result = VK_SUCCESS;

	m_swapchain.framebuffers.resize(m_swapchain.imageViews.size());

	// Attach image views to framebuffers
	for (int i = 0; i < m_swapchain.imageViews.size(); ++i)
	{
		std::array<VkImageView, 2> imageViews = { m_swapchain.imageViews[i], m_depthTexture.imageView };

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = m_renderPass;
		framebufferCreateInfo.attachmentCount = imageViews.size();
		framebufferCreateInfo.pAttachments = imageViews.data();
		framebufferCreateInfo.width = m_swapchain.extent.width;
		framebufferCreateInfo.height = m_swapchain.extent.height;
		framebufferCreateInfo.layers = 1;

		result = vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &m_swapchain.framebuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer");
			return result;
		}
	}

	return result;
}

VkResult 
VulkanRenderer::CreateCommandPool() 
{
	VkResult result = VK_SUCCESS;

	// Command pool for the graphics queue
	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {};
	graphicsCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicsCommandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily;

	result = vkCreateCommandPool(m_device, &graphicsCommandPoolCreateInfo, nullptr, &m_graphicsCommandPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool.");
		return result;
	}

	return result;
}

VkResult 
VulkanRenderer::CreateDepthResources() 
{
	VkFormat depthFormat = FindDepthFormat(m_physicalDevice);

	CreateImage(
		m_swapchain.extent.width,
		m_swapchain.extent.height,
		1, // only a 2D depth image
		VK_IMAGE_TYPE_2D,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_depthImage,
		m_depthImageMemory
	);
	CreateImageView(
		m_depthImage,
		VK_IMAGE_VIEW_TYPE_2D,
		depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		m_depthImageView
	);

	TransitionImageLayout(
		m_depthImage,
		depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::CreateVertexBuffer()
{
	for (GeometryData* geomData : m_scene->m_geometriesData)
	{
		GeometryBuffer geomBuffer;

		// ----------- Vertex attributes --------------

		std::vector<Byte>& indexData = geomData->vertexData.at(INDEX);
		VkDeviceSize indexBufferSize = sizeof(indexData[0]) * indexData.size();
		VkDeviceSize indexBufferOffset = 0;
		std::vector<Byte>& positionData = geomData->vertexData.at(POSITION);
		VkDeviceSize positionBufferSize = sizeof(positionData[0]) * positionData.size();
		VkDeviceSize positionBufferOffset = indexBufferSize;
		std::vector<Byte>& normalData = geomData->vertexData.at(NORMAL);
		VkDeviceSize normalBufferSize = sizeof(normalData[0]) * normalData.size();
		VkDeviceSize normalBufferOffset = positionBufferOffset + positionBufferSize;

		VkDeviceSize bufferSize = indexBufferSize + positionBufferSize + normalBufferSize;
		geomBuffer.bufferLayout.indexBufferOffset = indexBufferOffset;
		geomBuffer.bufferLayout.vertexBufferOffsets.insert(std::make_pair(POSITION, positionBufferOffset));
		geomBuffer.bufferLayout.vertexBufferOffsets.insert(std::make_pair(NORMAL, normalBufferOffset));

		// Stage buffer memory on host
		// We want staging so that we can map the vertex data on the host but
		// then transfer it to the device local memory for faster performance
		// This is the recommended way to allocate buffer memory,
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		CreateBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			stagingBuffer
		);

		// Allocate memory for the buffer
		CreateMemory(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory
		);

		// Bind buffer with memory
		VkDeviceSize memoryOffset = 0;
		vkBindBufferMemory(m_device, stagingBuffer, stagingBufferMemory, memoryOffset);

		// Filling the stage buffer with data
		void* data;
		vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indexData.data(), static_cast<size_t>(indexBufferSize));
		memcpy((Byte*)data + positionBufferOffset, positionData.data(), static_cast<size_t>(positionBufferSize));
		memcpy((Byte*)data + normalBufferOffset, normalData.data(), static_cast<size_t>(normalBufferSize));
		vkUnmapMemory(m_device, stagingBufferMemory);

		// -----------------------------------------

		CreateBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			geomBuffer.vertexBuffer
		);

		// Allocate memory for the buffer
		CreateMemory(
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			geomBuffer.vertexBuffer,
			geomBuffer.vertexBufferMemory
		);

		// Bind buffer with memory
		vkBindBufferMemory(m_device, geomBuffer.vertexBuffer, geomBuffer.vertexBufferMemory, memoryOffset);

		// Copy over to vertex buffer in device local memory
		CopyBuffer(geomBuffer.vertexBuffer, stagingBuffer, bufferSize);

		// Cleanup staging buffer memory
		vkDestroyBuffer(m_device, stagingBuffer, nullptr);
		vkFreeMemory(m_device, stagingBufferMemory, nullptr);

		m_geometryBuffers.push_back(geomBuffer);
	}
	return VK_SUCCESS;
}


VkResult 
VulkanRenderer::CreateUniformBuffer()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	VkDeviceSize memoryOffset = 0;
	CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		m_uniformStagingBuffer
	);

	// Allocate memory for the buffer
	CreateMemory(
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		m_uniformStagingBuffer,
		m_uniformStagingBufferMemory
	);

	// Bind buffer with memory
	vkBindBufferMemory(m_device, m_uniformStagingBuffer, m_uniformStagingBufferMemory, memoryOffset);

	CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		m_uniformBuffer
	);

	// Allocate memory for the buffer
	CreateMemory(
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_uniformBuffer,
		m_uniformBufferMemory
	);

	// Bind buffer with memory
	vkBindBufferMemory(m_device, m_uniformBuffer, m_uniformBufferMemory, memoryOffset);

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::CreateDescriptorPool() 
{
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes = &poolSize;
	poolCreateInfo.maxSets = 1;

	VkResult result = vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_descriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor pool");
		return result;
	}

	return result;
}

VkResult 
VulkanRenderer::CreateDescriptorSet() 
{
	
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descriptorSetLayout;

	VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor set");
		return result;
	}

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = m_uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(UniformBufferObject);

	// Update descriptor set info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = m_descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0; // descriptor set could be an array
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);


	return result;
}

VkResult 
VulkanRenderer::CreateCommandBuffers()
{
	m_commandBuffers.resize(m_swapchain.framebuffers.size());
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_graphicsCommandPool;
	// Primary means that can be submitted to a queue, but cannot be called from other command buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command buffers.");
		return result;
	}

	for (int i = 0; i < m_commandBuffers.size(); ++i)
	{
		// Begin command recording
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo);

		// Begin renderpass
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = m_renderPass;
		renderPassBeginInfo.framebuffer = m_swapchain.framebuffers[i];

		// The area where load and store takes place
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = m_swapchain.extent;

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassBeginInfo.clearValueCount = clearValues.size();
		renderPassBeginInfo.pClearValues = clearValues.data();

		// Record begin renderpass
		vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Record binding the graphics pipeline
		vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

		for (int b = 0; b < m_geometryBuffers.size(); ++b)
		{
			GeometryBuffer& geomBuffer = m_geometryBuffers[b];

			// Bind vertex buffer
			VkBuffer vertexBuffers[] = { geomBuffer.vertexBuffer, geomBuffer.vertexBuffer };
			VkDeviceSize offsets[] = { geomBuffer.bufferLayout.vertexBufferOffsets.at(POSITION), geomBuffer.bufferLayout.vertexBufferOffsets.at(NORMAL) };
			vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 2, vertexBuffers, offsets);

			// Bind index buffer
			vkCmdBindIndexBuffer(m_commandBuffers[i], geomBuffer.vertexBuffer, geomBuffer.bufferLayout.indexBufferOffset, VK_INDEX_TYPE_UINT16);

			// Bind uniform buffer
			vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

			// Record draw command for the triangle!
			vkCmdDrawIndexed(m_commandBuffers[i], m_scene->m_geometriesData[b]->vertexAttributes.at(INDEX).count, 1, 0, 0, 0);
		}

		// Record end renderpass
		vkCmdEndRenderPass(m_commandBuffers[i]);

		// End command recording
		result = vkEndCommandBuffer(m_commandBuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to record command buffers");
			return result;
		}
	}

	return result;
}

VkResult 
VulkanRenderer::CreateSemaphores() 
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult result = vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphore);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create imageAvailable semaphore");
		return result;
	}

	result = vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphore);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create renderFinished semaphore");
		return result;
	}

	return result;
}

VkResult
VulkanRenderer::CreateShaderModule(
	const std::vector<char>& code
	, VkShaderModule& shaderModule
)
{
	VkResult result = VK_SUCCESS;

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = (uint32_t*)code.data();

	result = vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return result;
}


void
VulkanRenderer::Update()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float timeSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

	UniformBufferObject ubo = {};
	timeSeconds = 0.0f;
	ubo.model = glm::rotate(glm::mat4(1.0f), timeSeconds * glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * 
		glm::scale(glm::mat4(1.0f), glm::vec3(3, 3, 3));
	ubo.view = glm::lookAt(glm::vec3(0.0f, 1.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), (float)m_swapchainExtent.width / (float)m_swapchainExtent.height, 0.001f, 10000.0f);

	// The Vulkan's Y coordinate is flipped from OpenGL (glm design), so we need to invert that
	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(m_device, m_uniformStagingBufferMemory, 0, sizeof(UniformBufferObject), 0, &data);
	memcpy(data, &ubo, sizeof(UniformBufferObject));
	vkUnmapMemory(m_device, m_uniformStagingBufferMemory);

	CopyBuffer(m_uniformBuffer, m_uniformStagingBuffer, sizeof(UniformBufferObject));
}

void 
VulkanRenderer::Render() 
{
	// Acquire the swapchain
	uint32_t imageIndex;
	vkAcquireNextImageKHR(
		m_device, 
		m_swapchain.swapchain, 
		60 * 1000000, // Timeout
		m_imageAvailableSemaphore, 
		VK_NULL_HANDLE, 
		&imageIndex
		);

	// Submit command buffers
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// Semaphore to wait on
	VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores; // The semaphore to wait on
	submitInfo.pWaitDstStageMask = waitStages; // At which stage to wait on
	
	// The command buffer to submit
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

	// Semaphore to signal
	VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// Submit to queue
	VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit queue");
	}

	// Present swapchain image!
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { m_swapchain.swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
}

uint32_t
VulkanRenderer::GetMemoryType(
	uint32_t typeFilter
	, VkMemoryPropertyFlags propertyFlags
	) const 
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

	for (auto i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		// Loop through each memory type and find a match
		if (typeFilter & (1 << i)) 
		{
			if (memProperties.memoryTypes[i].propertyFlags & propertyFlags) {
				return i;
			}
		}
	}

	throw std::runtime_error("Failed to find a suitable memory type");
}

void
VulkanRenderer::CreateBuffer(
	const VkDeviceSize size,
	const VkBufferUsageFlags usage,
	VkBuffer& buffer
	) const
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;

	// For multipurpose buffers, OR the VK_BUFFER_USAGE_ bits together
	bufferCreateInfo.usage = usage;

	// Buffer is used exclusively by the graphics queue
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create buffer");
	}
}

void 
VulkanRenderer::CreateMemory(
	const VkMemoryPropertyFlags memoryProperties,
	const VkBuffer& buffer,
	VkDeviceMemory& memory
) const 
{
	VkMemoryRequirements memoryRequirements = {};
	vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;

	// *N.B*
	// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT means memory allocated  can be mapped for host access using vkMapMemory
	// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ensures mapped memory matches allocated memory. 
	// Does not require flushing and invalidate cache before reading from mapped memory
	// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT uses the device local memorycr
	memoryAllocInfo.memoryTypeIndex =
		GetMemoryType(memoryRequirements.memoryTypeBits,
			memoryProperties);

	VkResult result = vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &memory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for buffer");
	}
}

void 
VulkanRenderer::CopyBuffer(
	VkBuffer dstBuffer, 
	VkBuffer srcBuffer, 
	VkDeviceSize size
	) const 
{
	VkCommandBuffer copyCommandBuffer = BeginSingleTimeCommands();

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(copyCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(copyCommandBuffer);
}

void 
VulkanRenderer::CreateImage(
	uint32_t width, 
	uint32_t height, 
	uint32_t depth,
	VkImageType imageType,
	VkFormat format, 
	VkImageTiling tiling, 
	VkImageUsageFlags usage, 
	VkMemoryPropertyFlags memPropertyFlags, 
	VkImage& image, 
	VkDeviceMemory& imageMemory
	) 
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = imageType;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = depth;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // For multisampling
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // used only by one queue that supports transfer operations
	imageInfo.flags = 0; // We might look into this for flags that support sparse image (if we need to do voxel 3D texture for volumetric)

	VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, &image);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_device, image, &memRequirements);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = GetMemoryType(
		memRequirements.memoryTypeBits, 
		memPropertyFlags
		);

	result = vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for image");
	}

	vkBindImageMemory(m_device, image, imageMemory, 0);
}

void 
VulkanRenderer::CreateImageView(
	const VkImage& image, 
	VkImageViewType viewType,
	VkFormat format, 
	VkImageAspectFlags aspectFlags,
	VkImageView& imageView
	) 
{
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.viewType = viewType; // 1D, 2D, 3D textures or cubemap

														  // Use default mapping for swizzle
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// The subresourcerange field is used to specify the purpose of this image view
	// https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkImageSubresourceRange
	imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags; // Use as color, depth, or stencil targets
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1; // Could have more if we're doing stereoscopic rendering

	VkResult result = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create VkImageView");
	}
}

void 
VulkanRenderer::TransitionImageLayout(
	VkImage image, 
	VkFormat format, 
	VkImageAspectFlags aspectMask,
	VkImageLayout oldLayout, 
	VkImageLayout newLayout
	) 
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Using image memory barrier to transition for image layout. 
	// There is a buffer memory barrier equivalent
	VkImageMemoryBarrier imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // (this isn't the default value so we must set it)
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = aspectMask;
	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		if (DepthFormatHasStencilComponent(format)) {
			imageBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.layerCount = 1;

	// Assign correct mask access
	if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED &&
		newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED &&
		newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		imageBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		imageBarrier.srcAccessMask = 0;
		imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	} else {
		throw std::invalid_argument("Unsupported layout transition");
	}

	// A pipeline barrier inserts an execution dependency and 
	// a set of memory dependencies between a set of commands earlier 
	// in the command buffer and a set of commands later in the command buffer. 
	// \ref https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#vkCmdPipelineBarrier
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Happen immediately on the pipeline
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageBarrier
	);

	EndSingleTimeCommands(commandBuffer);
}

void 
VulkanRenderer::CopyImage(
	VkImage dstImage, 
	VkImage srcImage, 
	uint32_t width, 
	uint32_t height
	) 
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Subresource is sort of like a buffer for images
	VkImageSubresourceLayers subResource = {};
	subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResource.baseArrayLayer = 0;
	subResource.mipLevel = 0;
	subResource.layerCount = 1;

	VkImageCopy region = {};
	region.srcSubresource = subResource;
	region.dstSubresource = subResource;
	region.srcOffset = { 0, 0 };
	region.dstOffset = { 0, 0 };
	region.extent.width = width;
	region.extent.height = height;
	region.extent.depth = 1;

	vkCmdCopyImage(
		commandBuffer,
		srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	EndSingleTimeCommands(commandBuffer);
}

VkCommandBuffer 
VulkanRenderer::BeginSingleTimeCommands() const {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_graphicsCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void 
VulkanRenderer::EndSingleTimeCommands(
	VkCommandBuffer commandBuffer
	) const 
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_graphicsQueue);

	vkFreeCommandBuffers(m_device, m_graphicsCommandPool, 1, &commandBuffer);
}
