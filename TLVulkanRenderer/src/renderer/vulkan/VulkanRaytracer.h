#pragma once
#include "VulkanRenderer.h"
#include "VulkanBuffer.h"

class VulkanRaytracer : public VulkanRenderer {
	
public:
	VulkanRaytracer(
		GLFWwindow* window,
		Scene* scene
	);

	virtual void
		Update() final;

	virtual void
		Render() final;

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

	void
	PrepareComputeDescriptors();

	void
	PrepareComputeCommandPool();

	void
	PrepareComputeStorageBuffer();

	void
	PrepareComputeUniformBuffer();

	VkResult
	PrepareRayTraceTextureResources();

	VkResult
	PrepareComputePipeline();

	VkResult
	PrepareComputeCommandBuffers();

	struct Quad {
		std::vector<uint16_t> indices;
		std::vector<vec2> positions;
		std::vector<vec2> uvs;
	} m_quad;


	struct Compute
	{
		// -- Compute compatible queue
		VkQueue queue;
		VkFence fence;

		// -- Descriptor
		VkDescriptorPool descriptorPool;
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSet descriptorSets;

		// -- Pipeline
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;

		// -- Commands
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;

		struct {
			// -- Uniform buffer
			VulkanBuffer::StorageBuffer uniform;
			VkDeviceMemory uniformMemory;

			// -- Shapes buffers
			VulkanBuffer::StorageBuffer indices;
			VulkanBuffer::StorageBuffer verticePositions;
			VulkanBuffer::StorageBuffer verticeNormals;

		} buffers;

		// -- Output storage image
		VulkanImage::Image storageRaytraceImage;

		struct UBOCompute
		{							// Compute shader uniform block object
			glm::vec3 lightPos = glm::vec3(1.0f, 2.0f, -10.0f);
			glm::vec3 position = glm::vec3(0.0f, 0.0f, 10.0f);
			glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);;
			glm::vec3 lookat = glm::vec3(0.0f);
			glm::vec3 forward;
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec2 pixelLength;
			float fov = 40.0f;
			float aspectRatio = 45.0f;
		} ubo;
	} m_compute;
};
