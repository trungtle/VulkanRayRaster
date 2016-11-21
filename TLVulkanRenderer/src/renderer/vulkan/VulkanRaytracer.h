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
	// GRAPHICS PIPELINE
	// -----------
	void
	PrepareGraphics() final;

	VkResult
	PrepareGraphicsPipeline() final;

	VkResult
	PrepareGraphicsVertexBuffer() final;

	// --- Descriptor

	VkResult
	PrepareGraphicsDescriptorPool() final;

	VkResult
	PrepareGraphicsDescriptorSetLayout() final;

	VkResult
	PrepareGraphicsDescriptorSets() final;

	// --- Command buffers

	VkResult
	PrepareGraphicsCommandBuffers() final;

	// -----------
	// COMPUTE PIPELINE (for raytracing)
	// -----------

	void
	PrepareCompute() final;

	VkResult
	PrepareRayTraceTextureResources();

	VkResult
	PrepareComputePipeline();

	VkResult
	PrepareComputeCommandBuffers();


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

	} compute;


};
