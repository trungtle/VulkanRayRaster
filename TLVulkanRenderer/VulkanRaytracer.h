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


	struct
	{
		// -- Compute compatible queue
		VkQueue queue;

		// -- Descriptor
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSet descriptorSets;

		// -- Pipeline
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;

		struct {
			// -- Uniform buffer
			VkBuffer uniform;

			// -- Triangle buffers
			VkBuffer vertexBuffer;
		} buffer;

		// -- Output storage image
		Texture m_rayTracedTexture;

	} compute;


};
