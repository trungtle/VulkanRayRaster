#include <assert.h>
#include <iostream>
#include <set>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

#include "VulkanRenderer.h"
#include "Utilities.h"
#include "VulkanImage.h"
#include "VulkanBuffer.h"
#include "VulkanVertex.h"

VulkanRenderer::VulkanRenderer(
	GLFWwindow* window,
	Scene* scene
	)
	:
	Renderer(window, scene)
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
	m_vulkanDevice = new VulkanDevice(m_window, "Vulkan renderer", m_logger);

	// Grabs the first queue in the graphics queue family since we only need one graphics queue anyway
	vkGetDeviceQueue(m_vulkanDevice->device, m_vulkanDevice->queueFamilyIndices.graphicsFamily, 0, &graphics.m_graphicsQueue);

	// Grabs the first queue in the present queue family since we only need one present queue anyway
	vkGetDeviceQueue(m_vulkanDevice->device, m_vulkanDevice->queueFamilyIndices.presentFamily, 0, &m_presentQueue);

	VkResult result;

	result = PrepareDescriptorPool();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created descriptor pool");

	result = PrepareDescriptorSetLayout();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created descriptor set layout");

	result = PrepareCommandPool();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created command pool");

	result = PrepareRenderPass();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created renderpass");

	result = PrepareDepthResources();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created depth image");

	result = PrepareImageViews();
	assert(result == VK_SUCCESS);
	m_logger->info("Created {} VkImageViews", m_vulkanDevice->m_swapchain.imageViews.size());

	result = PrepareFramebuffers();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created framebuffers");

	result = PrepareGraphicsPipeline();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created graphics pipeline");

	result = PrepareVertexBuffer();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created vertex buffer");

	result = PrepareUniformBuffer();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created uniform buffer");

	result = PrepareGraphicsDescriptorSet();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created descriptor set");

	result = PrepareGraphicsCommandBuffers();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created command buffers");

	result = PrepareSemaphores();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Created semaphores");

}


VulkanRenderer::~VulkanRenderer()
{
	// Free memory in the opposite order of creation

	vkDestroySemaphore(m_vulkanDevice->device, m_imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(m_vulkanDevice->device, m_renderFinishedSemaphore, nullptr);
	
	vkFreeCommandBuffers(m_vulkanDevice->device, graphics.m_graphicsCommandPool, graphics.m_commandBuffers.size(), graphics.m_commandBuffers.data());
	
	vkDestroyDescriptorPool(m_vulkanDevice->device, graphics.m_descriptorPool, nullptr);

	vkDestroyImageView(m_vulkanDevice->device, graphics.m_depthTexture.imageView, nullptr);
	vkDestroyImage(m_vulkanDevice->device, graphics.m_depthTexture.image, nullptr);
	vkFreeMemory(m_vulkanDevice->device, graphics.m_depthTexture.imageMemory, nullptr);

	for (VulkanBuffer::GeometryBuffer& geomBuffer : graphics.m_geometryBuffers) {
		vkFreeMemory(m_vulkanDevice->device, geomBuffer.vertexBufferMemory, nullptr);
		vkDestroyBuffer(m_vulkanDevice->device, geomBuffer.vertexBuffer, nullptr);
	}

	vkFreeMemory(m_vulkanDevice->device, graphics.m_uniformStagingBufferMemory, nullptr);
	vkDestroyBuffer(m_vulkanDevice->device, graphics.m_uniformStagingBuffer, nullptr);
	vkFreeMemory(m_vulkanDevice->device, graphics.m_uniformBufferMemory, nullptr);
	vkDestroyBuffer(m_vulkanDevice->device, graphics.m_uniformBuffer, nullptr);
	
	vkDestroyCommandPool(m_vulkanDevice->device, graphics.m_graphicsCommandPool, nullptr);
	for (auto& frameBuffer : m_vulkanDevice->m_swapchain.framebuffers) {
		vkDestroyFramebuffer(m_vulkanDevice->device, frameBuffer, nullptr);
	}
	
	vkDestroyRenderPass(m_vulkanDevice->device, graphics.m_renderPass, nullptr);
	
	vkDestroyDescriptorSetLayout(m_vulkanDevice->device, graphics.m_descriptorSetLayout, nullptr);
	
	vkDestroyPipelineLayout(m_vulkanDevice->device, graphics.m_pipelineLayout, nullptr);
	for (auto& imageView : m_vulkanDevice->m_swapchain.imageViews) {
        vkDestroyImageView(m_vulkanDevice->device, imageView, nullptr);
    }
	vkDestroyPipeline(m_vulkanDevice->device, graphics.m_graphicsPipeline, nullptr);
	
	delete m_vulkanDevice;
}




VkResult 
VulkanRenderer::PrepareImageViews() 
{
    VkResult result = VK_SUCCESS;

	m_vulkanDevice->m_swapchain.imageViews.resize(m_vulkanDevice->m_swapchain.images.size());
    for (auto i = 0; i < m_vulkanDevice->m_swapchain.imageViews.size(); ++i) {
		CreateImageView(
			m_vulkanDevice->m_swapchain.images[i],
			VK_IMAGE_VIEW_TYPE_2D,
			m_vulkanDevice->m_swapchain.imageFormat,
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_vulkanDevice->m_swapchain.imageViews[i]
		);
    }
    return result;
}

VkResult 
VulkanRenderer::PrepareRenderPass() 
{
	// ----- Specify color attachment ------

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_vulkanDevice->m_swapchain.imageFormat;
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
	depthAttachment.format = VulkanImage::FindDepthFormat(m_vulkanDevice->physicalDevice);
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

	VkResult result = vkCreateRenderPass(m_vulkanDevice->device, &renderPassCreateInfo, nullptr, &graphics.m_renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass");
		return result;
	}

	return result;
}

VkResult 
VulkanRenderer::PrepareDescriptorSetLayout() 
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

	VkResult result = vkCreateDescriptorSetLayout(m_vulkanDevice->device, &layoutInfo, nullptr, &graphics.m_descriptorSetLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor set layout");
		return result;
	}
	
	return result;
}

VkResult 
VulkanRenderer::PrepareGraphicsPipeline() {
	
	VkResult result = VK_SUCCESS;

	// Load SPIR-V bytecode
	// The SPIR_V files can be compiled by running glsllangValidator.exe from the VulkanSDK or
	// by invoking the custom script shaders/compileShaders.bat
	
	// -- Load vertex shader
	VkShaderModule vertShader;
	PrepareShaderModule(
		"shaders/vert.spv",
		vertShader
	);
	m_logger->info("Loaded {} vertex shader", "shaders/vert.spv");

	// -- Load fragment shader
	VkShaderModule fragShader;
	PrepareShaderModule(
		"shaders/frag.spv",
		fragShader
	);
	m_logger->info("Loaded {} frag shader", "shaders/frag.spv");


	// -- Setup the programmable stages for the pipeline. This links the shader modules with their corresponding stages.
	// \ref https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineShaderStageCreateInfo
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo = MakePipelineShaderStageCreateInfo(
		VK_SHADER_STAGE_VERTEX_BIT,
		vertShader
		);
	
	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = MakePipelineShaderStageCreateInfo(
		VK_SHADER_STAGE_FRAGMENT_BIT,
		fragShader
		);

	// -- Setup the fixed functions stages for the pipeline.
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineVertexInputStateCreateInfo
	// 1. Vertex input stage
	VkPipelineVertexInputStateCreateInfo vertexInputStageCreateInfo = {};
	vertexInputStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	// Input binding description
	VkVertexInputBindingDescription bindingDesc[2] = {
		VulkanVertex::GetVertexInputBindingDescription(0, m_scene->m_geometriesData[0]->vertexAttributes.at(POSITION)),
		VulkanVertex::GetVertexInputBindingDescription(1, m_scene->m_geometriesData[0]->vertexAttributes.at(NORMAL))
	};
	vertexInputStageCreateInfo.vertexBindingDescriptionCount = 2;
	vertexInputStageCreateInfo.pVertexBindingDescriptions = bindingDesc;
	
	// Attribute description (position, normal, texcoord etc.)
	auto vertAttribDesc = VulkanVertex::GetVertexInputAttributeDescriptions();
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
	viewport.width = static_cast<float>(m_vulkanDevice->m_swapchain.extent.width);
	viewport.height = static_cast<float>(m_vulkanDevice->m_swapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = m_vulkanDevice->m_swapchain.extent;

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
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = MakePipelineLayoutCreateInfo(&graphics.m_descriptorSetLayout);
	pipelineLayoutCreateInfo.pSetLayouts = &graphics.m_descriptorSetLayout;
	CheckVulkanResult(
		vkCreatePipelineLayout(m_vulkanDevice->device, &pipelineLayoutCreateInfo, nullptr, &graphics.m_pipelineLayout),
		"Failed to create pipeline layout."
		);

	// Finally, create our graphics pipeline here!
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderCreateInfos = { vertShaderStageCreateInfo, fragShaderStageCreateInfo };
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = shaderCreateInfos.size(); // Number of shader stages
	graphicsPipelineCreateInfo.pStages = shaderCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStageCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr; // Skipped
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = nullptr; // Skipped
	graphicsPipelineCreateInfo.layout = graphics.m_pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = graphics.m_renderPass;
	graphicsPipelineCreateInfo.subpass = 0; // Index to the subpass we'll be using

	// Since pipelins are expensive to create, potentially we could reuse a common parent pipeline.
	// We just have one here so we don't need to specify these values.
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	// We can also cache the pipeline object and store it in a file for resuse
	result = vkCreateGraphicsPipelines(m_vulkanDevice->device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphics.m_graphicsPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline");
		return result;
	}

	// We don't need the shader modules after the graphics pipeline creation. Destroy them now.
	vkDestroyShaderModule(m_vulkanDevice->device, vertShader, nullptr);
	vkDestroyShaderModule(m_vulkanDevice->device, fragShader, nullptr);

	return result;
}

VkResult 
VulkanRenderer::PrepareComputePipeline() 
{

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::PrepareFramebuffers() 
{
	VkResult result = VK_SUCCESS;

	m_vulkanDevice->m_swapchain.framebuffers.resize(m_vulkanDevice->m_swapchain.imageViews.size());

	// Attach image views to framebuffers
	for (int i = 0; i < m_vulkanDevice->m_swapchain.imageViews.size(); ++i)
	{
		std::array<VkImageView, 2> imageViews = { m_vulkanDevice->m_swapchain.imageViews[i], graphics.m_depthTexture.imageView };

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = graphics.m_renderPass;
		framebufferCreateInfo.attachmentCount = imageViews.size();
		framebufferCreateInfo.pAttachments = imageViews.data();
		framebufferCreateInfo.width = m_vulkanDevice->m_swapchain.extent.width;
		framebufferCreateInfo.height = m_vulkanDevice->m_swapchain.extent.height;
		framebufferCreateInfo.layers = 1;

		result = vkCreateFramebuffer(m_vulkanDevice->device, &framebufferCreateInfo, nullptr, &m_vulkanDevice->m_swapchain.framebuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer");
			return result;
		}
	}

	return result;
}

VkResult 
VulkanRenderer::PrepareCommandPool() 
{
	VkResult result = VK_SUCCESS;

	// Command pool for the graphics queue
	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = MakeCommandPoolCreateInfo(m_vulkanDevice->queueFamilyIndices.graphicsFamily);

	result = vkCreateCommandPool(m_vulkanDevice->device, &graphicsCommandPoolCreateInfo, nullptr, &graphics.m_graphicsCommandPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool.");
		return result;
	}

	return result;
}

VkResult 
VulkanRenderer::PrepareDepthResources() 
{
	VkFormat depthFormat = VulkanImage::FindDepthFormat(m_vulkanDevice->physicalDevice);

	CreateImage(
		m_vulkanDevice->m_swapchain.extent.width,
		m_vulkanDevice->m_swapchain.extent.height,
		1, // only a 2D depth image
		VK_IMAGE_TYPE_2D,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		graphics.m_depthTexture.image,
		graphics.m_depthTexture.imageMemory
	);
	CreateImageView(
		graphics.m_depthTexture.image,
		VK_IMAGE_VIEW_TYPE_2D,
		depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		graphics.m_depthTexture.imageView
	);

	TransitionImageLayout(
		graphics.m_depthTexture.image,
		depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::PrepareVertexBuffer()
{
	for (GeometryData* geomData : m_scene->m_geometriesData)
	{
		VulkanBuffer::GeometryBuffer geomBuffer;

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
		geomBuffer.bufferLayout.vertexBufferOffsets.insert(std::make_pair(INDEX, indexBufferOffset));
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
		vkBindBufferMemory(m_vulkanDevice->device, stagingBuffer, stagingBufferMemory, memoryOffset);

		// Filling the stage buffer with data
		void* data;
		vkMapMemory(m_vulkanDevice->device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indexData.data(), static_cast<size_t>(indexBufferSize));
		memcpy((Byte*)data + positionBufferOffset, positionData.data(), static_cast<size_t>(positionBufferSize));
		memcpy((Byte*)data + normalBufferOffset, normalData.data(), static_cast<size_t>(normalBufferSize));
		vkUnmapMemory(m_vulkanDevice->device, stagingBufferMemory);

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
		vkBindBufferMemory(m_vulkanDevice->device, geomBuffer.vertexBuffer, geomBuffer.vertexBufferMemory, memoryOffset);

		// Copy over to vertex buffer in device local memory
		CopyBuffer(geomBuffer.vertexBuffer, stagingBuffer, bufferSize);

		// Cleanup staging buffer memory
		vkDestroyBuffer(m_vulkanDevice->device, stagingBuffer, nullptr);
		vkFreeMemory(m_vulkanDevice->device, stagingBufferMemory, nullptr);

		graphics.m_geometryBuffers.push_back(geomBuffer);
	}
	return VK_SUCCESS;
}


VkResult 
VulkanRenderer::PrepareUniformBuffer()
{
	VkDeviceSize bufferSize = sizeof(GraphicsUniformBufferObject);
	VkDeviceSize memoryOffset = 0;
	CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		graphics.m_uniformStagingBuffer
	);

	// Allocate memory for the buffer
	CreateMemory(
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		graphics.m_uniformStagingBuffer,
		graphics.m_uniformStagingBufferMemory
	);

	// Bind buffer with memory
	vkBindBufferMemory(m_vulkanDevice->device, graphics.m_uniformStagingBuffer, graphics.m_uniformStagingBufferMemory, memoryOffset);

	CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		graphics.m_uniformBuffer
	);

	// Allocate memory for the buffer
	CreateMemory(
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		graphics.m_uniformBuffer,
		graphics.m_uniformBufferMemory
	);

	// Bind buffer with memory
	vkBindBufferMemory(m_vulkanDevice->device, graphics.m_uniformBuffer, graphics.m_uniformBufferMemory, memoryOffset);

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::PrepareDescriptorPool() 
{
	VkDescriptorPoolSize poolSize = MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = MakeDescriptorPoolCreateInfo(1, &poolSize, 1);

	CheckVulkanResult(
		vkCreateDescriptorPool(m_vulkanDevice->device, &descriptorPoolCreateInfo, nullptr, &graphics.m_descriptorPool),
		"Failed to create descriptor pool"
	);

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::PrepareGraphicsDescriptorSet() 
{
	VkDescriptorSetAllocateInfo allocInfo = MakeDescriptorSetAllocateInfo(graphics.m_descriptorPool, &graphics.m_descriptorSetLayout);

	CheckVulkanResult(
		vkAllocateDescriptorSets(m_vulkanDevice->device, &allocInfo, &graphics.m_descriptorSet),
		"Failed to allocate descriptor set"
	);

	VkDescriptorBufferInfo bufferInfo = MakeDescriptorBufferInfo(graphics.m_uniformBuffer, 0, sizeof(GraphicsUniformBufferObject));

	// Update descriptor set info
	VkWriteDescriptorSet descriptorWrite = MakeWriteDescriptorSet(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		graphics.m_descriptorSet,
		0,
		1,
		&bufferInfo,
		nullptr
		);

	vkUpdateDescriptorSets(m_vulkanDevice->device, 1, &descriptorWrite, 0, nullptr);

	return VK_SUCCESS;
}

VkResult 
VulkanRenderer::PrepareGraphicsCommandBuffers()
{
	graphics.m_commandBuffers.resize(m_vulkanDevice->m_swapchain.framebuffers.size());
	// Primary means that can be submitted to a queue, but cannot be called from other command buffers
	VkCommandBufferAllocateInfo allocInfo = MakeCommandBufferAllocateInfo(graphics.m_graphicsCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_vulkanDevice->m_swapchain.framebuffers.size());

	VkResult result = vkAllocateCommandBuffers(m_vulkanDevice->device, &allocInfo, graphics.m_commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command buffers.");
		return result;
	}

	for (int i = 0; i < graphics.m_commandBuffers.size(); ++i)
	{
		// Begin command recording
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		vkBeginCommandBuffer(graphics.m_commandBuffers[i], &beginInfo);

		// Begin renderpass
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = graphics.m_renderPass;
		renderPassBeginInfo.framebuffer = m_vulkanDevice->m_swapchain.framebuffers[i];

		// The area where load and store takes place
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = m_vulkanDevice->m_swapchain.extent;

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassBeginInfo.clearValueCount = clearValues.size();
		renderPassBeginInfo.pClearValues = clearValues.data();

		// Record begin renderpass
		vkCmdBeginRenderPass(graphics.m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Record binding the graphics pipeline
		vkCmdBindPipeline(graphics.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.m_graphicsPipeline);

		for (int b = 0; b <graphics.m_geometryBuffers.size(); ++b)
		{
			VulkanBuffer::GeometryBuffer& geomBuffer = graphics.m_geometryBuffers[b];

			// Bind vertex buffer
			VkBuffer vertexBuffers[] = { geomBuffer.vertexBuffer, geomBuffer.vertexBuffer };
			VkDeviceSize offsets[] = { geomBuffer.bufferLayout.vertexBufferOffsets.at(POSITION), geomBuffer.bufferLayout.vertexBufferOffsets.at(NORMAL) };
			vkCmdBindVertexBuffers(graphics.m_commandBuffers[i], 0, 2, vertexBuffers, offsets);

			// Bind index buffer
			vkCmdBindIndexBuffer(graphics.m_commandBuffers[i], geomBuffer.vertexBuffer, geomBuffer.bufferLayout.vertexBufferOffsets.at(INDEX), VK_INDEX_TYPE_UINT16);

			// Bind uniform buffer
			vkCmdBindDescriptorSets(graphics.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.m_pipelineLayout, 0, 1, &graphics.m_descriptorSet, 0, nullptr);

			// Record draw command for the triangle!
			vkCmdDrawIndexed(graphics.m_commandBuffers[i], m_scene->m_geometriesData[b]->vertexAttributes.at(INDEX).count, 1, 0, 0, 0);
		}

		// Record end renderpass
		vkCmdEndRenderPass(graphics.m_commandBuffers[i]);

		// End command recording
		result = vkEndCommandBuffer(graphics.m_commandBuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to record command buffers");
			return result;
		}
	}

	return result;
}

VkResult 
VulkanRenderer::PrepareSemaphores() 
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult result = vkCreateSemaphore(m_vulkanDevice->device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphore);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create imageAvailable semaphore");
		return result;
	}

	result = vkCreateSemaphore(m_vulkanDevice->device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphore);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create renderFinished semaphore");
		return result;
	}

	return result;
}

VkResult
VulkanRenderer::PrepareShaderModule(
	const std::string& filepath, 
	VkShaderModule& shaderModule
	) const 
{
	std::vector<Byte> bytecode;
	LoadSPIR_V(filepath.c_str(), bytecode);

	VkResult result = VK_SUCCESS;

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = bytecode.size();
	shaderModuleCreateInfo.pCode = (uint32_t*)bytecode.data();

	result = vkCreateShaderModule(m_vulkanDevice->device, &shaderModuleCreateInfo, nullptr, &shaderModule);
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

	GraphicsUniformBufferObject ubo = {};
	ubo.model = glm::rotate(glm::mat4(1.0f), timeSeconds * glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * 
		glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
	ubo.view = glm::lookAt(glm::vec3(0.0f, 1.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), (float)m_vulkanDevice->m_swapchain.extent.width / (float)m_vulkanDevice->m_swapchain.extent.height, 0.001f, 10000.0f);

	// The Vulkan's Y coordinate is flipped from OpenGL (glm design), so we need to invert that
	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(m_vulkanDevice->device, graphics.m_uniformStagingBufferMemory, 0, sizeof(GraphicsUniformBufferObject), 0, &data);
	memcpy(data, &ubo, sizeof(GraphicsUniformBufferObject));
	vkUnmapMemory(m_vulkanDevice->device, graphics.m_uniformStagingBufferMemory);

	CopyBuffer(graphics.m_uniformBuffer, graphics.m_uniformStagingBuffer, sizeof(GraphicsUniformBufferObject));
}

void 
VulkanRenderer::Render() 
{
	// Acquire the swapchain
	uint32_t imageIndex;
	vkAcquireNextImageKHR(
		m_vulkanDevice->device, 
		m_vulkanDevice->m_swapchain.swapchain, 
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
	submitInfo.pCommandBuffers = &graphics.m_commandBuffers[imageIndex];

	// Semaphore to signal
	VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// Submit to queue
	VkResult result = vkQueueSubmit(graphics.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit queue");
	}

	// Present swapchain image!
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { m_vulkanDevice->m_swapchain.swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(graphics.m_graphicsQueue, &presentInfo);
}

uint32_t
VulkanRenderer::GetMemoryType(
	uint32_t typeFilter
	, VkMemoryPropertyFlags propertyFlags
	) const 
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vulkanDevice->physicalDevice, &memProperties);

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

	CheckVulkanResult(
		vkCreateBuffer(m_vulkanDevice->device, &bufferCreateInfo, nullptr, &buffer),
		"Failed to create buffer"
		);
}

void 
VulkanRenderer::CreateMemory(
	const VkMemoryPropertyFlags memoryProperties,
	const VkBuffer& buffer,
	VkDeviceMemory& memory
) const 
{
	VkMemoryRequirements memoryRequirements = {};
	vkGetBufferMemoryRequirements(m_vulkanDevice->device, buffer, &memoryRequirements);

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

	VkResult result = vkAllocateMemory(m_vulkanDevice->device, &memoryAllocInfo, nullptr, &memory);
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

	CheckVulkanResult(
		vkCreateImage(m_vulkanDevice->device, &imageInfo, nullptr, &image),
		"Failed to create image"
	);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_vulkanDevice->device, image, &memRequirements);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = GetMemoryType(
		memRequirements.memoryTypeBits, 
		memPropertyFlags
		);

	CheckVulkanResult(
		vkAllocateMemory(m_vulkanDevice->device, &memoryAllocInfo, nullptr, &imageMemory),
		"Failed to allocate memory for image"
		);

	CheckVulkanResult(
		vkBindImageMemory(m_vulkanDevice->device, image, imageMemory, 0),
		"Failed to bind image memory"
		);
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

	VkResult result = vkCreateImageView(m_vulkanDevice->device, &imageViewCreateInfo, nullptr, &imageView);
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
		if (VulkanImage::DepthFormatHasStencilComponent(format)) {
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
	} else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		imageBarrier.srcAccessMask = 0;
		imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
	allocInfo.commandPool = graphics.m_graphicsCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_vulkanDevice->device, &allocInfo, &commandBuffer);

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

	vkQueueSubmit(graphics.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics.m_graphicsQueue);

	vkFreeCommandBuffers(m_vulkanDevice->device, graphics.m_graphicsCommandPool, 1, &commandBuffer);
}
