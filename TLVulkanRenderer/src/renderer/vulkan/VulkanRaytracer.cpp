#include "VulkanRaytracer.h"
#include "Utilities.h"

VulkanRaytracer::VulkanRaytracer(
	GLFWwindow* window, 
	Scene* scene): VulkanRenderer(window, scene) 
{
	VkResult result;

//	result = CreateRayTraceTextureResources();
//	assert(result == VK_SUCCESS);
//	m_logger->info<std::string>("Create ray trace texture resources");

//	PrepareUniformBuffer();
//	PrepareDescriptorPool();
//	PrepareComputePipeline();
}

VulkanRaytracer::~VulkanRaytracer() 
{
	vkFreeCommandBuffers(m_vulkanDevice->device, compute.commandPool, compute.commandBuffers.size(), compute.commandBuffers.data());
	vkDestroyCommandPool(m_vulkanDevice->device, compute.commandPool, nullptr);

	vkFreeDescriptorSets(m_vulkanDevice->device, compute.descriptorPool, 1, &compute.descriptorSets);
	vkDestroyDescriptorSetLayout(m_vulkanDevice->device, compute.descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(m_vulkanDevice->device, compute.descriptorPool, nullptr);

	vkDestroyImageView(m_vulkanDevice->device, compute.storageRaytraceImage.imageView, nullptr);
	vkDestroyImage(m_vulkanDevice->device, compute.storageRaytraceImage.image, nullptr);
	vkFreeMemory(m_vulkanDevice->device, compute.storageRaytraceImage.imageMemory, nullptr);
	
	vkDestroyBuffer(m_vulkanDevice->device, compute.buffers.uniform.buffer, nullptr);
	vkDestroyBuffer(m_vulkanDevice->device, compute.buffers.triangles.buffer, nullptr);

}

VkResult
VulkanRaytracer::CreateRayTraceTextureResources()
{
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	CreateImage(
		m_vulkanDevice->m_swapchain.extent.width,
		m_vulkanDevice->m_swapchain.extent.height,
		1, // only a 2D depth image
		VK_IMAGE_TYPE_2D,
		imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		// Image is sampled in fragment shader and used as storage for compute output
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		compute.storageRaytraceImage.image,
		compute.storageRaytraceImage.imageMemory
	);
	CreateImageView(
		compute.storageRaytraceImage.image,
		VK_IMAGE_VIEW_TYPE_2D,
		imageFormat,
		VK_IMAGE_ASPECT_COLOR_BIT,
		compute.storageRaytraceImage.imageView
	);

	TransitionImageLayout(
		compute.storageRaytraceImage.image,
		imageFormat,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	// Create sampler
	MakeDefaultTextureSampler(m_vulkanDevice->device, &compute.storageRaytraceImage.sampler);

	// Initialize descriptor
	compute.storageRaytraceImage.descriptor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compute.storageRaytraceImage.descriptor.imageView = compute.storageRaytraceImage.imageView;
	compute.storageRaytraceImage.descriptor.sampler = compute.storageRaytraceImage.sampler;

	return VK_SUCCESS;
}

VkResult
VulkanRaytracer::PrepareDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		// Uniform buffer for compute
		MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
		// Image sampler
		MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4),
		// Output storage image of ray traced result
		MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1),
		// Mesh storage buffers
		MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = MakeDescriptorPoolCreateInfo(
		poolSizes.size(),
		poolSizes.data(),
		3
	);

	CheckVulkanResult(
		vkCreateDescriptorPool(m_vulkanDevice->device, &descriptorPoolCreateInfo, nullptr, &compute.descriptorPool),
		"Failed to create descriptor pool"
	);

	return VK_SUCCESS;
}


VkResult 
VulkanRaytracer::PrepareGraphicsPipeline() 
{
	return VK_SUCCESS;
}

VkResult 
VulkanRaytracer::PrepareComputePipeline() 
{
	// 1. Get compute queue

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.queueFamilyIndex = m_vulkanDevice->queueFamilyIndices.computeFamily;
	vkGetDeviceQueue(m_vulkanDevice->device, m_vulkanDevice->queueFamilyIndices.computeFamily, 0, &compute.queue);

	// 2. Create descriptor set layout

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: output storage image
		MakeDescriptorSetLayoutBinding(
			0,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_COMPUTE_BIT
		),
		// Binding 1: uniform buffer for compute
		MakeDescriptorSetLayoutBinding(
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT
		),
		// Binding 2: uniform buffer for triangles
		MakeDescriptorSetLayoutBinding(
			2,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT
		)
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
		MakeDescriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			setLayoutBindings.size()
		);

	CheckVulkanResult(
		vkCreateDescriptorSetLayout(m_vulkanDevice->device, &descriptorSetLayoutCreateInfo, nullptr, &compute.descriptorSetLayout),
		"Failed to create descriptor set layout"
	);

	// 3. Allocate descriptor set

	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = MakeDescriptorSetAllocateInfo(compute.descriptorPool, &compute.descriptorSetLayout);

	CheckVulkanResult(
		vkAllocateDescriptorSets(m_vulkanDevice->device, &descriptorSetAllocInfo, &compute.descriptorSets),
		"failed to allocate descriptor set"
		);

	// 4. Update descriptor sets

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0, output storage image
		MakeWriteDescriptorSet(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			compute.descriptorSets,
			0, // Binding 0
			1,
			nullptr,
			&compute.storageRaytraceImage.descriptor
		),
		MakeWriteDescriptorSet(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			compute.descriptorSets,
			1, // Binding 1
			1,
			&compute.buffers.uniform.descriptor,
			nullptr
		),
		MakeWriteDescriptorSet(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			compute.descriptorSets,
			2, // Binding 2
			1,
			&compute.buffers.triangles.descriptor,
			nullptr
		)
	};

	vkUpdateDescriptorSets(m_vulkanDevice->device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

	// 5. Create pipeline layout

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = MakePipelineLayoutCreateInfo(&compute.descriptorSetLayout);
	CheckVulkanResult(
		vkCreatePipelineLayout(m_vulkanDevice->device, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayout),
		"Failed to create pipeline layout"
	);

	// 6. Create compute shader pipeline
	VkComputePipelineCreateInfo computePipelineCreateInfo = MakeComputePipelineCreateInfo(compute.pipelineLayout, 0);

	// Create shader modules from bytecodes
	VkShaderModule raytraceShader;
	PrepareShaderModule(
		"shaders/raytracing/raytrace.spv",
		raytraceShader
	);
	m_logger->info("Loaded {} vertex shader", "shaders/raytracing/raytrace/raytrace.spv");


	computePipelineCreateInfo.stage = MakePipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, raytraceShader);

	CheckVulkanResult(
		vkCreateComputePipelines(m_vulkanDevice->device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &compute.pipeline),
		"Failed to create compute pipeline"
	);

	// 7. Create compute command pool 
	// (separately from graphics command pool in case that they're not on the same queue)
	VkCommandPoolCreateInfo commandPoolCreateInfo = MakeCommandPoolCreateInfo(m_vulkanDevice->queueFamilyIndices.computeFamily);

	CheckVulkanResult(
		vkCreateCommandPool(m_vulkanDevice->device, &commandPoolCreateInfo, nullptr, &compute.commandPool),
		"Failed to create command pool for compute"
	);

	// 8. Create compute command buffers

	VkCommandBufferAllocateInfo commandBufferAllocInfo = MakeCommandBufferAllocateInfo(compute.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

	CheckVulkanResult(
		vkAllocateCommandBuffers(m_vulkanDevice->device, &commandBufferAllocInfo, compute.commandBuffers.data()),
		"Failed to allocate compute command buffers"
	);

	// 9. Record compute commands

	return VK_SUCCESS;
}

VkResult 
VulkanRaytracer::PrepareGraphicsCommandBuffers() 
{
	return VK_SUCCESS;
}

VkResult 
VulkanRaytracer::PrepareComputeCommandBuffers() {

	return VK_SUCCESS;
}

void 
VulkanRaytracer::Compute::PrepareUniformBuffer() 
{
//	CreateBuffer(
//		sizeof(GraphicsUniformBufferObject),
//		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//		buffers.uniform.buffer
//	);
}
