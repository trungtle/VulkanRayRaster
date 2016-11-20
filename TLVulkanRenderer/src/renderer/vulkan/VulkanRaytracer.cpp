#include "VulkanRaytracer.h"
#include "Utilities.h"
#include "VulkanVertex.h"

VulkanRaytracer::VulkanRaytracer(
	GLFWwindow* window, 
	Scene* scene): VulkanRenderer(window, scene) 
{
//	result = CreateRayTraceTextureResources();
//	m_logger->info<std::string>("Create ray trace texture resources");

	//PrepareVertexBuffer();
	//PrepareDescriptorPool();
	//PrepareDescriptorSetLayout();
	//PrepareGraphicsDescriptorSets();
	//PrepareGraphicsPipeline();
	//PrepareGraphicsCommandBuffers();

//	PrepareUniformBuffer();
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
VulkanRaytracer::PrepareVertexBuffer() 
{
	graphics.m_geometryBuffers.clear();

	const std::vector<float> positions = {
		-1.0, -1.0,
		1.0, -1.0,
		1.0,  1.0,
		1.0,  1.0,
		-1.0,  1.0,
		-1.0, -1.0
	};

	const std::vector<float> uvs = {
		-1.0, -1.0,
		1.0, -1.0,
		1.0,  1.0,
		1.0,  1.0,
		-1.0,  1.0,
		-1.0, -1.0
	};

	VulkanBuffer::GeometryBuffer geomBuffer;

	// ----------- Vertex attributes --------------

	VkDeviceSize positionBufferSize = sizeof(positions[0]) * positions.size();
	VkDeviceSize positionBufferOffset = 0;
	VkDeviceSize uvBufferSize = sizeof(uvs[0]) * uvs.size();
	VkDeviceSize uvBufferOffset = positionBufferSize;

	VkDeviceSize bufferSize = positionBufferSize + uvBufferSize;
	geomBuffer.bufferLayout.vertexBufferOffsets.insert(std::make_pair(POSITION, positionBufferOffset));
	geomBuffer.bufferLayout.vertexBufferOffsets.insert(std::make_pair(TEXCOORD, uvBufferOffset));

	// Stage buffer memory on host
	// We want staging so that we can map the vertex data on the host but
	// then transfer it to the device local memory for faster performance
	// This is the recommended way to allocate buffer memory,
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	m_vulkanDevice->CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		stagingBuffer
	);

	// Allocate memory for the buffer
	m_vulkanDevice->CreateMemory(
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
	memcpy((Byte*)data, positions.data(), static_cast<size_t>(positionBufferSize));
	memcpy((Byte*)data + positionBufferSize, uvs.data(), static_cast<size_t>(uvBufferSize));
	vkUnmapMemory(m_vulkanDevice->device, stagingBufferMemory);

	// -----------------------------------------

	m_vulkanDevice->CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		geomBuffer.vertexBuffer
	);

	// Allocate memory for the buffer
	m_vulkanDevice->CreateMemory(
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		geomBuffer.vertexBuffer,
		geomBuffer.vertexBufferMemory
	);

	// Bind buffer with memory
	vkBindBufferMemory(m_vulkanDevice->device, geomBuffer.vertexBuffer, geomBuffer.vertexBufferMemory, memoryOffset);

	// Copy over to vertex buffer in device local memory
	m_vulkanDevice->CopyBuffer(
		graphics.m_graphicsQueue,
		graphics.m_graphicsCommandPool,
		geomBuffer.vertexBuffer,
		stagingBuffer,
		bufferSize
	);

	// Cleanup staging buffer memory
	vkDestroyBuffer(m_vulkanDevice->device, stagingBuffer, nullptr);
	vkFreeMemory(m_vulkanDevice->device, stagingBufferMemory, nullptr);

	graphics.m_geometryBuffers.push_back(geomBuffer);

	return VK_SUCCESS;
}

VkResult
VulkanRaytracer::CreateRayTraceTextureResources()
{
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	m_vulkanDevice->CreateImage(
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
	m_vulkanDevice->CreateImageView(
		compute.storageRaytraceImage.image,
		VK_IMAGE_VIEW_TYPE_2D,
		imageFormat,
		VK_IMAGE_ASPECT_COLOR_BIT,
		compute.storageRaytraceImage.imageView
	);

	m_vulkanDevice->TransitionImageLayout(
		graphics.m_graphicsQueue,
		graphics.m_graphicsCommandPool,
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
		// Image sampler
		MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		MakeDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = MakeDescriptorPoolCreateInfo(
		poolSizes.size(),
		poolSizes.data(),
		3
	);

	CheckVulkanResult(
		vkCreateDescriptorPool(m_vulkanDevice->device, &descriptorPoolCreateInfo, nullptr, &graphics.descriptorPool),
		"Failed to create descriptor pool"
	);

	return VK_SUCCESS;
}

VkResult 
VulkanRaytracer::PrepareDescriptorSetLayout() 
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Fragment shader image sampler
		MakeDescriptorSetLayoutBinding(
			0,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT
		),
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
		MakeDescriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			setLayoutBindings.size()
		);

	CheckVulkanResult(
		vkCreateDescriptorSetLayout(m_vulkanDevice->device, &descriptorSetLayoutCreateInfo, nullptr, &graphics.descriptorSetLayout),
		"Failed to create descriptor set layout"
	);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = MakePipelineLayoutCreateInfo(&graphics.descriptorSetLayout);
	
	CheckVulkanResult(
		vkCreatePipelineLayout(m_vulkanDevice->device, &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout),
		"Failed to create pipeline layout"
	);

	return VK_SUCCESS;
}

VkResult 
VulkanRaytracer::PrepareGraphicsDescriptorSets() 
{
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = MakeDescriptorSetAllocateInfo(graphics.descriptorPool, &graphics.descriptorSetLayout);

	CheckVulkanResult(
		vkAllocateDescriptorSets(m_vulkanDevice->device, &descriptorSetAllocInfo, &graphics.descriptorSets),
		"failed to allocate descriptor sets"
	);

	return VK_SUCCESS;
}

VkResult 
VulkanRaytracer::PrepareGraphicsPipeline() 
{
	VkResult result = VK_SUCCESS;

	// Load SPIR-V bytecode
	// The SPIR_V files can be compiled by running glsllangValidator.exe from the VulkanSDK or
	// by invoking the custom script shaders/compileShaders.bat
	VkShaderModule vertShader;
	PrepareShaderModule("shaders/raytracing/raytrace.vert.spv", vertShader);
	VkShaderModule fragShader;
	PrepareShaderModule("shaders/raytracing/raytrace.frag.spv", fragShader);

	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineVertexInputStateCreateInfo
	// 1. Vertex input stage
	VkPipelineVertexInputStateCreateInfo vertexInputStageCreateInfo = MakePipelineVertexInputStateCreateInfo();

	// Input binding description
	VkVertexInputBindingDescription bindingDesc[2] = {
		VulkanVertex::GetVertexInputBindingDescription(0, m_scene->m_geometriesData[0]->vertexAttributes.at(POSITION)),
		VulkanVertex::GetVertexInputBindingDescription(1, m_scene->m_geometriesData[0]->vertexAttributes.at(TEXCOORD))
	};
	vertexInputStageCreateInfo.vertexBindingDescriptionCount = 2;
	vertexInputStageCreateInfo.pVertexBindingDescriptions = bindingDesc;

	// Attribute description (position, normal, texcoord etc.)
	auto vertAttribDesc = VulkanVertex::GetVertexInputAttributeDescriptions();
	vertexInputStageCreateInfo.vertexAttributeDescriptionCount = vertAttribDesc.size();
	vertexInputStageCreateInfo.pVertexAttributeDescriptions = vertAttribDesc.data();

	// 2. Input assembly
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineInputAssemblyStateCreateInfo
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
		MakePipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	// 3. Skip tesselation 
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineTessellationStateCreateInfo


	// 4. Viewports and scissors
	// Viewport typically just covers the entire swapchain extent
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineViewportStateCreateInfo
	std::vector<VkViewport> viewports = {
		MakeFullscreenViewport(m_vulkanDevice->m_swapchain.extent)
	};

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = m_vulkanDevice->m_swapchain.extent;

	std::vector<VkRect2D> scissors = {
		scissor
	};

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = MakePipelineViewportStateCreateInfo(viewports, scissors);

	// 5. Rasterizer
	// This stage converts primitives into fragments. It also performs depth/stencil testing, face culling, scissor test.
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineRasterizationStateCreateInfo 
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = MakePipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

	// 6. Multisampling state. We're not doing anything special here for now
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineMultisampleStateCreateInfo

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
		MakePipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

	// 6. Depth/stecil tests state
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineDepthStencilStateCreateInfo
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = MakePipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER);

	// 7. Color blending state
	// \see https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineColorBlendStateCreateInfo
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// Do minimal work for now, so no blending. This will be useful for later on when we want to do pixel blending (alpha blending).
	// There are a lot of interesting blending we can do here.
	colorBlendAttachmentState.blendEnable = VK_FALSE;

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {
		colorBlendAttachmentState
	};

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = MakePipelineColorBlendStateCreateInfo(colorBlendAttachments);

	// 8. Dynamic state. Some pipeline states can be updated dynamically. Skip for now.

	// 9. Create pipeline layout to hold uniforms. This can be modified dynamically. 
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = MakePipelineLayoutCreateInfo(&graphics.descriptorSetLayout);
	pipelineLayoutCreateInfo.pSetLayouts = &graphics.descriptorSetLayout;
	CheckVulkanResult(
		vkCreatePipelineLayout(m_vulkanDevice->device, &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout),
		"Failed to create pipeline layout."
	);

	// -- Setup the programmable stages for the pipeline. This links the shader modules with their corresponding stages.
	// \ref https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#VkPipelineShaderStageCreateInfo

	std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfos = {
		MakePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShader),
		MakePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)
	};

	// Finally, create our graphics pipeline here!

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo =
		MakeGraphicsPipelineCreateInfo(
			shaderCreateInfos,
			&vertexInputStageCreateInfo,
			&inputAssemblyStateCreateInfo,
			nullptr,
			&viewportStateCreateInfo,
			&rasterizationStateCreateInfo,
			&colorBlendStateCreateInfo,
			&multisampleStateCreateInfo,
			&depthStencilStateCreateInfo,
			nullptr,
			graphics.pipelineLayout,
			graphics.m_renderPass,
			0, // Subpass

			   // Since pipelins are expensive to create, potentially we could reuse a common parent pipeline using the base pipeline handle.									
			   // We just have one here so we don't need to specify these values.
			VK_NULL_HANDLE,
			-1
		);

	// We can also cache the pipeline object and store it in a file for resuse
	CheckVulkanResult(
		vkCreateGraphicsPipelines(
			m_vulkanDevice->device,
			VK_NULL_HANDLE,					// Pipeline caches here
			1,								// Pipeline count
			&graphicsPipelineCreateInfo,
			nullptr,
			&graphics.m_graphicsPipeline	// Pipelines
		),
		"Failed to create graphics pipeline"
	);

	// We don't need the shader modules after the graphics pipeline creation. Destroy them now.
	vkDestroyShaderModule(m_vulkanDevice->device, vertShader, nullptr);
	vkDestroyShaderModule(m_vulkanDevice->device, fragShader, nullptr);

	return result;

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
		"shaders/raytracing/raytrace.comp.spv",
		raytraceShader
	);
	m_logger->info("Loaded {} comp shader", "shaders/raytracing/raytrace/raytrace.spv");


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
			VkDeviceSize offsets[] = { geomBuffer.bufferLayout.vertexBufferOffsets.at(POSITION), geomBuffer.bufferLayout.vertexBufferOffsets.at(TEXCOORD) };
			vkCmdBindVertexBuffers(graphics.m_commandBuffers[i], 0, 2, vertexBuffers, offsets);

			// Record draw command for the triangle!
			vkCmdDraw(graphics.m_commandBuffers[i], 6, 1, 0, 0);
		}

		// Record end renderpass
		vkCmdEndRenderPass(graphics.m_commandBuffers[i]);

		// End command recording
		result = vkEndCommandBuffer(graphics.m_commandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffers");
			return result;
		}
	}

	return result;
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
