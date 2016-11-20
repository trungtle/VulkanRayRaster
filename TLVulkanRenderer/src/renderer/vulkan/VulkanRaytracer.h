#pragma once
#include "VulkanRenderer.h"
#include "VulkanBuffer.h"

class VulkanRaytracer : public VulkanRenderer {
	
public:
	VulkanRaytracer(
		GLFWwindow* window,
		Scene* scene
	);

	virtual ~VulkanRaytracer() final;

protected:

	// -----------
	// BUFFER
	// -----------
	VkResult
	PrepareVertexBuffer() final;

	// -----------
	// DESCRIPTOR
	// -----------

	VkResult
	PrepareDescriptorPool() final;

	VkResult
	PrepareDescriptorSetLayout() final;

	VkResult
	PrepareGraphicsDescriptorSets() final;

	VkResult
	CreateRayTraceTextureResources();

	// -----------
	// PIPELINE
	// -----------

	VkResult
	PrepareGraphicsPipeline() final;

	VkResult
	PrepareComputePipeline() final;

	// -----------
	// COMMANDS
	// -----------

	VkResult
	PrepareGraphicsCommandBuffers() final;

	VkResult
	PrepareComputeCommandBuffers() final;


	struct Compute
	{
		// -- Compute compatible queue
		VkQueue queue;

		// -- Descriptor
		VkDescriptorPool descriptorPool;
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSet descriptorSets;

		// -- Pipeline
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;

		// -- Commands
		VkCommandPool commandPool;
		std::vector<VkCommandBuffer> commandBuffers;

		struct {
			// -- Uniform buffer
			VulkanBuffer::StorageBuffer uniform;

			// -- Triangle buffers
			VulkanBuffer::StorageBuffer triangles;
		} buffers;

		// -- Output storage image
		VulkanImage::Image storageRaytraceImage;

		void PrepareUniformBuffer();

	} compute;


};
