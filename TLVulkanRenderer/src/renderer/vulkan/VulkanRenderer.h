/**
 * The basic of this is heavily referenced at:
 *  - Majority of this application was modified from Vulkan Tutorial(https://vulkan-tutorial.com/) by Alexander Overvoorde. [Github](https://github.com/Overv/VulkanTutorial). 
 *  - WSI Tutorial by Chris Hebert.
 *  - https://github.com/SaschaWillems/Vulkan by Sascha Willems.
 */

#pragma once

#define GLFW_INCLUDE_VULKAN
#include "spdlog/spdlog.h"
#include "renderer/Renderer.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanUtil.h"
#include "Typedef.h"
#include "VulkanImage.h"
#include "VulkanBuffer.h"

using namespace VulkanUtil;
using namespace VulkanUtil::Make;

struct GraphicsUniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

// ===================
// VULKAN RENDERER
// ===================

/**
 * \brief 
 */
class VulkanRenderer : public Renderer
{
public:
	VulkanRenderer(
		GLFWwindow* window,
		Scene* scene
		);

	virtual ~VulkanRenderer() override;

	virtual void
	Update() override;

    virtual void
    Render() override;

protected:
	
    VkResult
    PrepareImageViews();

	VkResult
	PrepareRenderPass();

	// ----------------
	// PIPELINES
	// ----------------

	/**
	 * \brief The graphics pipeline are often fixed. Create a new pipeline if we need a different pipeline settings
	 * \return 
	 */
	virtual VkResult
	PrepareGraphicsPipeline();

	virtual VkResult
	PrepareComputePipeline();

	VkResult
	PrepareShaderModule(
		const std::string& filepath,
		VkShaderModule& shaderModule
		) const;

	VkResult
	PrepareFramebuffers();

	VkResult
	PrepareDepthResources();

	VkResult
	PrepareVertexBuffer();

	VkResult
	PrepareUniformBuffer();

	// -----------
	// DESCRIPTOR
	// -----------

	virtual VkResult
	PrepareDescriptorPool();

	VkResult
	PrepareGraphicsDescriptorSet();

	VkResult
	PrepareDescriptorSetLayout();

	// --------------
	// COMMAND BUFFERS
	// ---------------
	
	/**
	* \brief Vulkan commands are created in advance and submitted to the queue,
	*        instead of using direct function calls.
	* \return
	*/
	VkResult
	PrepareCommandPool();

	virtual VkResult
	PrepareGraphicsCommandBuffers();

	virtual VkResult
	PrepareComputeCommandBuffers() {
		return VK_SUCCESS;
	};

	// ----------------
	// SYCHRONIZATION
	// ----------------

	VkResult
	PrepareSemaphores();

	/**
	* \brief Helper to determine the memory type to allocate from our graphics card
	* \param typeFilter
	* \param propertyFlags
	* \return
	*/
	uint32_t
	GetMemoryType(
		uint32_t typeFilter
		, VkMemoryPropertyFlags propertyFlags
	) const;


	void
	CreateBuffer(
		const VkDeviceSize size,
		const VkBufferUsageFlags usage,
		VkBuffer& buffer
	) const;

	void
	CreateMemory(
		const VkMemoryPropertyFlags memoryProperties,
		const VkBuffer& buffer,
		VkDeviceMemory& memory
	) const;

	void
	CopyBuffer(
		VkBuffer dstBuffer,
		VkBuffer srcBuffer,
		VkDeviceSize size
	) const;

	void
	CreateImage(
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		VkImageType imageType,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory
	);

	void
	CreateImageView(
		const VkImage& image,
		VkImageViewType viewType,
		VkFormat format,
		VkImageAspectFlags aspectFlags,
		VkImageView& imageView
	);

	void
	TransitionImageLayout(
		VkImage image,
		VkFormat format,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldLayout,
		VkImageLayout newLayout
		);

	void
	CopyImage(
		VkImage dstImage,
		VkImage srcImage,
		uint32_t width,
		uint32_t height
		);

	VkCommandBuffer 
	BeginSingleTimeCommands() const;

	void 
	EndSingleTimeCommands(
		VkCommandBuffer commandBuffer
		) const;

	VulkanDevice* m_vulkanDevice;

	/**
	* \brief Handles to the Vulkan present queue. This may or may not be the same as the graphics queue
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#devsandqueues-queues
	*/
	VkQueue m_presentQueue;

	struct {
		
		/**
		* \brief Descriptor set layout to decribe our resource binding (ex. UBO)
		*/
		VkDescriptorSetLayout m_descriptorSetLayout;

		/**
		* \brief Descriptor pool for our resources
		*/
		VkDescriptorPool m_descriptorPool;

		/**
		* \brief Descriptor set for our resources
		*/
		VkDescriptorSet m_descriptorSet;

		/**
		* \brief This describes the uniforms inside shaders
		*/
		VkPipelineLayout m_pipelineLayout;

		/**
		* \brief Holds the renderpass object. This also represents the framebuffer attachments
		*/
		VkRenderPass m_renderPass;

		VulkanImage::Image m_depthTexture;

		std::vector<VulkanBuffer::GeometryBuffer> m_geometryBuffers;
		
		/**
		* \brief Uniform buffers
		*/
		VkBuffer m_uniformStagingBuffer;
		VkBuffer m_uniformBuffer;
		VkDeviceMemory m_uniformStagingBufferMemory;
		VkDeviceMemory m_uniformBufferMemory;

		/**
		* \brief Graphics pipeline
		*/
		VkPipeline m_graphicsPipeline;

		/**
		* \brief Command pool
		*/
		VkCommandPool m_graphicsCommandPool;

		/**
		* \brief Command buffers to record our commands
		*/
		std::vector<VkCommandBuffer> m_commandBuffers;

		/**
		* \brief Handles to the Vulkan graphics queue. This may or may not be the same as the present queue
		* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#devsandqueues-queues
		*/
		VkQueue m_graphicsQueue;

	} graphics;

	/**
	 * \brief Semaphores to signal when to acquire and present swapchain images
	 */
	VkSemaphore m_imageAvailableSemaphore;
	VkSemaphore m_renderFinishedSemaphore;

    /**
     * \brief Logger
     */
    std::shared_ptr<spdlog::logger> m_logger;
};


