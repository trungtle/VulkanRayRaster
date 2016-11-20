#include "VulkanRaytracer.h"
#include "Utilities.h"

VulkanRaytracer::VulkanRaytracer(
	GLFWwindow* window, 
	Scene* scene): VulkanRenderer(window, scene) 
{
	VkResult result;

	result = CreateRayTraceTextureResources();
	assert(result == VK_SUCCESS);
	m_logger->info<std::string>("Create ray trace texture resources");
}

VulkanRaytracer::~VulkanRaytracer() 
{
	vkDestroyImageView(m_device, compute.m_rayTracedTexture.imageView, nullptr);
	vkDestroyImage(m_device, compute.m_rayTracedTexture.image, nullptr);
	vkFreeMemory(m_device, compute.m_rayTracedTexture.imageMemory, nullptr);
}

VkResult
VulkanRaytracer::CreateRayTraceTextureResources()
{
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	CreateImage(
		m_swapchain.extent.width,
		m_swapchain.extent.height,
		1, // only a 2D depth image
		VK_IMAGE_TYPE_2D,
		imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		// Image is sampled in fragment shader and used as storage for compute output
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		compute.m_rayTracedTexture.image,
		compute.m_rayTracedTexture.imageMemory
	);
	CreateImageView(
		compute.m_rayTracedTexture.image,
		VK_IMAGE_VIEW_TYPE_2D,
		imageFormat,
		VK_IMAGE_ASPECT_COLOR_BIT,
		compute.m_rayTracedTexture.imageView
	);

	TransitionImageLayout(
		compute.m_rayTracedTexture.image,
		imageFormat,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	// Create sampler
	MakeDefaultTextureSampler(m_device, &compute.m_rayTracedTexture.sampler);

	// Initialize descriptor
	compute.m_rayTracedTexture.descriptor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compute.m_rayTracedTexture.descriptor.imageView = compute.m_rayTracedTexture.imageView;
	compute.m_rayTracedTexture.descriptor.sampler = compute.m_rayTracedTexture.sampler;

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
		vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool),
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
	queueCreateInfo.queueFamilyIndex = m_queueFamilyIndices.computeFamily;
	vkGetDeviceQueue(m_device, m_queueFamilyIndices.computeFamily, 0, &compute.queue);

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBinding = {
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
			setLayoutBinding.data(),
			setLayoutBinding.size()
		);

	// 2. Create descriptor set layout

	CheckVulkanResult(
		vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &compute.descriptorSetLayout),
		"Failed to create descriptor set layout"
	);

	// 3. Create pipeline layout

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = MakePipelineLayoutCreateInfo(&compute.descriptorSetLayout);
	CheckVulkanResult(
		vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayout),
		"Failed to create pipeline layout"
	);

	// 4. Allocate descriptor set

	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = MakeDescriptorSetAllocateInfo(m_descriptorPool, &compute.descriptorSetLayout);

	CheckVulkanResult(
		vkAllocateDescriptorSets(m_device, &descriptorSetAllocInfo, &compute.descriptorSets),
		"failed to allocate descriptor set"
		);

	// 5. Update descriptor sets

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0, output storage image
		MakeWriteDescriptorSet(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			compute.descriptorSets, 
			0, // Binding 0
			1, 
			nullptr,
			&compute.m_rayTracedTexture.descriptor
			),
		MakeWriteDescriptorSet(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			compute.descriptorSets,
			1, // Binding 1
			1,
			&compute.buffer.uniformBuffer.descriptor,
			nullptr
			),
		MakeWriteDescriptorSet(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			compute.descriptorSets,
			2, // Binding 2
			1,
			&compute.buffer.trianglesBuffer.descriptor,
			nullptr
			)
	};

	vkUpdateDescriptorSets(m_device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

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
		vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &compute.pipeline),
		"Failed to create compute pipeline"
	);

	// 7. Create compute command pool

	// 8. Create compute command buffers

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
