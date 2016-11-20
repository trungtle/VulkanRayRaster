#pragma once
#include "VulkanRenderer.h"

class VulkanRaytracer : public VulkanRenderer {
	
public:
	VulkanRaytracer(
		GLFWwindow* window,
		Scene* scene
	);

	virtual ~VulkanRaytracer() final;

protected:

	// -----------
	// DESCRIPTOR
	// -----------

	virtual VkResult
	PrepareDescriptorPool() override;

	VkResult
	CreateRayTraceTextureResources();

	VkResult
	PrepareGraphicsPipeline() override;

	VkResult
	PrepareComputePipeline() override;

	// -----------
	// PIPELINE
	// -----------

	VkResult
	PrepareGraphicsCommandBuffers() override;

	VkResult
	PrepareComputeCommandBuffers() override;


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
			StorageBuffer uniform;

			// -- Triangle buffers
			StorageBuffer triangles;
		} buffers;

		// -- Output storage image
		VulkanImage::Image storageRaytraceImage;

		void PrepareUniformBuffer();

	} compute;


};
