#include "VulkanDevice.h"
#include <set>
#include <iostream>

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
	if (func != nullptr)
	{
		return func(vkInstance, pCreateInfo, pAllocator, pCallback);
	}
	else
	{
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

VkResult
VulkanDevice::InitializeVulkanInstance()
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
	std::vector<const char*> extensions = VulkanUtil::GetInstanceRequiredExtensions(isEnableValidationLayers);

	// Create instance info struct
	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	// Grab validation layers
	if (isEnableValidationLayers)
	{
		assert(VulkanUtil::CheckValidationLayerSupport(VALIDATION_LAYERS));
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		instanceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	}
	else
	{
		instanceCreateInfo.enabledLayerCount = 0;
	}

	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

VkResult
VulkanDevice::SetupDebugCallback()
{

	// Do nothing if we have not enabled debug mode
	if (!isEnableValidationLayers)
	{
		return VK_SUCCESS;
	}

	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {};
	debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	// Report errors and warnings back
	debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	debugReportCallbackCreateInfo.pfnCallback = DebugCallbackFn;

	return CreateDebugReportCallbackEXT(instance, &debugReportCallbackCreateInfo, nullptr, &debugCallback);
}
VkResult
VulkanDevice::CreateWindowSurface(
	GLFWwindow* window
	)
{
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surfaceKHR);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface");
	}

	return result;
}


VkResult
VulkanDevice::SelectPhysicalDevice()
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	if (physicalDeviceCount == 0)
	{
		throw std::runtime_error("Failed to find a GPU that supports Vulkan");
	}

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	for (auto pd : physicalDevices)
	{
		if (VulkanUtil::IsDeviceVulkanCompatible(pd, surfaceKHR))
		{
			physicalDevice = pd;
			break;
		}
	}

	return physicalDevice != nullptr ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
}

VkResult
VulkanDevice::SetupLogicalDevice()
{
	// @todo: should add new & interesting features later
	VkPhysicalDeviceFeatures deviceFeatures = {};

	// Create logical device info struct and populate it
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	// Create a set of unique queue families for the required queues
	queueFamilyIndices = VulkanUtil::FindQueueFamilyIndices(physicalDevice, surfaceKHR);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily };
	for (auto i : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
		queueCreateInfo.queueCount = 1; // Only using one queue for now. We actually don't need that many.
		float queuePriority = 1.0f; // Queue priority. Required even if we only have one queue.
		queueCreateInfo.pQueuePriorities = &queuePriority;

		// Append to queue create infos
		queueCreateInfos.push_back(queueCreateInfo);
	}

	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

	// Grab logical device extensions
	std::vector<const char*> enabledExtensions = VulkanUtil::GetDeviceRequiredExtensions(physicalDevice);
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

	// Grab validation layers
	if (isEnableValidationLayers)
	{
		assert(VulkanUtil::CheckValidationLayerSupport(VALIDATION_LAYERS));
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		deviceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	}
	else
	{
		deviceCreateInfo.enabledLayerCount = 0;
	}

	// Create the logical device
	VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

	return result;
}

VkResult
VulkanDevice::PrepareSwapchain()
{
	// Query existing support
	VulkanUtil::Type::SwapchainSupport swapchainSupport = VulkanUtil::QuerySwapchainSupport(physicalDevice, surfaceKHR);

	// Select three settings:
	// 1. Surface format
	// 2. Present mode
	// 3. Extent
	// @todo Right now the preference is embedded inside these functions. We need to expose it to a global configuration file somewhere instead.
	VkSurfaceFormatKHR surfaceFormat = VulkanUtil::SelectDesiredSwapchainSurfaceFormat(swapchainSupport.surfaceFormats);
	VkPresentModeKHR presentMode = VulkanUtil::SelectDesiredSwapchainPresentMode(swapchainSupport.presentModes);
	VkExtent2D extent = VulkanUtil::SelectDesiredSwapchainExtent(swapchainSupport.capabilities);

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
	swapchainCreateInfo.surface = surfaceKHR;
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
	assert(queueFamilyIndices.IsComplete());
	if (queueFamilyIndices.presentFamily == queueFamilyIndices.graphicsFamily)
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
			static_cast<uint32_t>(queueFamilyIndices.graphicsFamily),
			static_cast<uint32_t>(queueFamilyIndices.presentFamily),
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
	VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &m_swapchain.swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swapchain!");
	}

	// Initialize the vector of swapchain images here. 
	uint32_t imageCount;
	vkGetSwapchainImagesKHR(device, m_swapchain.swapchain, &imageCount, nullptr);
	m_swapchain.images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, m_swapchain.swapchain, &imageCount, m_swapchain.images.data());

	// Initialize other swapchain related fields
	m_swapchain.imageFormat = surfaceFormat.format;
	m_swapchain.extent = extent;

	return result;
}


// =====================

void VulkanDevice::Initialize(GLFWwindow* window) 
{
	VkResult result = InitializeVulkanInstance();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Initalizes Vulkan instance");

	// Only enable the validation layer when running in debug mode
#ifdef NDEBUG
	isEnableValidationLayers = false;
#else
	isEnableValidationLayers = true;
#endif

	result = SetupDebugCallback();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Setup debug callback");

	result = CreateWindowSurface(window);
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created window surface");

	result = SelectPhysicalDevice();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Selected physical device");

	result = SetupLogicalDevice();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Setup logical device");

	result = PrepareSwapchain();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created swapchain");

}

void 
VulkanDevice::Destroy() 
{
	vkDestroySwapchainKHR(device, m_swapchain.swapchain, nullptr);

	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance, surfaceKHR, nullptr);

	DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);

	vkDestroyInstance(instance, nullptr);
}
