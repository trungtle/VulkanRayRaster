#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <map>
#include <glm/mat4x4.hpp>
#include "SceneUtil.h"

using namespace glm;

namespace VulkanUtil
{
	// -------- Type ----------- //

	namespace Type {

		struct Texture
		{
			int width;
			int height;
			VkImage image;
			VkImageView imageView;
			VkDeviceMemory imageMemory;
			VkSampler sampler;
			VkDescriptorImageInfo descriptor;
		};

		struct VertexAttributeDescriptions
		{

			VkVertexInputAttributeDescription position;
			VkVertexInputAttributeDescription normal;
			VkVertexInputAttributeDescription texcoord;

			std::array<VkVertexInputAttributeDescription, 2>
				ToArray() const
			{
				std::array<VkVertexInputAttributeDescription, 2> attribDesc = {
					position,
					normal
				};

				return attribDesc;
			}
		};

		// ===================
		// UNIFORM
		// ===================

		struct UniformBufferObject
		{
			glm::mat4 model;
			glm::mat4 view;
			glm::mat4 proj;
		};

		// ===================
		// BUFFER
		// ===================

		struct GeometryBufferOffset
		{
			std::map<EVertexAttributeType, VkDeviceSize> vertexBufferOffsets;
		};

		struct StorageBuffer
		{
			VkBuffer buffer;
			VkDescriptorBufferInfo descriptor;
		};
		// ===================
		// QUEUE
		// ===================

		/**
		* \brief A struct to store queue family indices
		*/
		struct QueueFamilyIndices
		{
			int graphicsFamily = -1;
			int presentFamily = -1;
			int computeFamily = -1;
			int transferFamily = -1;

			bool IsComplete() const {
				return graphicsFamily >= 0 && presentFamily >= 0 && computeFamily >= 0 && transferFamily >= 0;
			}
		};

		// ===================
		// SWAPCHAIN SUPPORT
		// ===================

		/**
		* \brief Struct to store available swapchain support
		*/
		struct SwapchainSupport
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> surfaceFormats;
			std::vector<VkPresentModeKHR> presentModes;

			bool IsComplete() const {
				return !surfaceFormats.empty() && !presentModes.empty();
			}
		};

		// ===================
		// GEOMETRIES
		// ===================
		struct GeometryBuffer
		{
			/**
			* \brief Byte offsets for vertex attributes and resource buffers into our unified buffer
			*/
			GeometryBufferOffset bufferLayout;

			/**
			* \brief Handle to the vertex buffers
			*/
			VkBuffer vertexBuffer;

			/**
			* \brief Handle to the device memory
			*/
			VkDeviceMemory vertexBufferMemory;
		};
	}

	namespace Make
	{
		// ===================
		// DESCRIPTOR
		// ===================

		VkDescriptorPoolSize
			MakeDescriptorPoolSize(
				VkDescriptorType descriptorType,
				uint32_t descriptorCount
			);

		VkDescriptorPoolCreateInfo
			MakeDescriptorPoolCreateInfo(
				uint32_t poolSizeCount,
				VkDescriptorPoolSize* poolSizes,
				uint32_t maxSets = 1
			);

		VkDescriptorSetLayoutBinding
			MakeDescriptorSetLayoutBinding(
				uint32_t binding,
				VkDescriptorType descriptorType,
				VkShaderStageFlags shaderFlags,
				uint32_t descriptorCount = 1
			);

		VkDescriptorSetLayoutCreateInfo
			MakeDescriptorSetLayoutCreateInfo(
				VkDescriptorSetLayoutBinding* bindings,
				uint32_t bindingCount = 1
			);

		VkDescriptorSetAllocateInfo
			MakeDescriptorSetAllocateInfo(
				VkDescriptorPool descriptorPool,
				VkDescriptorSetLayout* descriptorSetLayout,
				uint32_t descriptorSetCount = 1
			);

		VkDescriptorBufferInfo
			MakeDescriptorBufferInfo(
				VkBuffer buffer,
				VkDeviceSize offset,
				VkDeviceSize range
			);

		VkWriteDescriptorSet
			MakeWriteDescriptorSet(
				VkDescriptorType type,
				VkDescriptorSet dstSet,
				uint32_t dstBinding,
				uint32_t descriptorCount,
				VkDescriptorBufferInfo* bufferInfo,
				VkDescriptorImageInfo* imageInfo
			);

		// ===================
		// PIPELINE
		// ===================
		VkPipelineLayoutCreateInfo
			MakePipelineLayoutCreateInfo(
				VkDescriptorSetLayout* descriptorSetLayouts,
				uint32_t setLayoutCount = 1
			);

		VkPipelineShaderStageCreateInfo
			MakePipelineShaderStageCreateInfo(
				VkShaderStageFlagBits stage,
				const VkShaderModule& shaderModule
			);

		VkComputePipelineCreateInfo
			MakeComputePipelineCreateInfo(
				VkPipelineLayout layout,
				VkPipelineCreateFlags flags
			);

		// ===================
		// TEXTURE
		// ===================

		void
			MakeDefaultTextureSampler(
				const VkDevice& device,
				VkSampler* sampler
			);
		// ===================
		// COMMANDS
		// ===================
		VkCommandPoolCreateInfo
			MakeCommandPoolCreateInfo(
				uint32_t queueFamilyIndex
			);

		VkCommandBufferAllocateInfo
			MakeCommandBufferAllocateInfo(
				VkCommandPool commandPool,
				VkCommandBufferLevel level,
				uint32_t bufferCount
			);
	}

	inline void CheckVulkanResult(
		VkResult result,
		std::string message
	)
	{
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error(message);
		}
	}


	static
		VkVertexInputBindingDescription
		GetVertexInputBindingDescription(
			uint32_t binding,
			const VertexAttributeInfo& vertexAttrib
		)
	{
		VkVertexInputBindingDescription bindingDesc;
		bindingDesc.binding = binding; // Which index of the array of VkBuffer for vertices
		bindingDesc.stride = vertexAttrib.componentLength * vertexAttrib.componentTypeByteSize;
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDesc;
	}

	static
		std::array<VkVertexInputAttributeDescription, 2>
		GetAttributeDescriptions()
	{
		Type::VertexAttributeDescriptions attributeDesc;

		// Position attribute
		attributeDesc.position.binding = 0;
		attributeDesc.position.location = 0;
		attributeDesc.position.format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDesc.position.offset = 0;

		// Normal attribute
		attributeDesc.normal.binding = 1;
		attributeDesc.normal.location = 1;
		attributeDesc.normal.format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDesc.normal.offset = 0;

		// Texcoord attribute
		//attributeDesc.texcoord.binding = 0;
		//attributeDesc.texcoord.location = 2;
		//attributeDesc.texcoord.format = VK_FORMAT_R32G32_SFLOAT;
		//attributeDesc.texcoord.offset = vertexAttrib.at(TEXCOORD).byteOffset;

		return attributeDesc.ToArray();
	}

	Type::QueueFamilyIndices
		FindQueueFamilyIndices(
			const VkPhysicalDevice& physicalDevicece
			, const VkSurfaceKHR& surfaceKHR // For finding queue that can present image to our surface
		);



	// ===================
	// EXTENSIONS
	// ===================

	bool
		CheckValidationLayerSupport(
			const std::vector<const char*>& validationLayers
		);

	std::vector<const char*>
		GetInstanceRequiredExtensions(
			bool enableValidationLayers
		);

	std::vector<const char*>
		GetDeviceRequiredExtensions(
			const VkPhysicalDevice& physicalDevice
		);

	// ===================
	// DEVICE
	// ===================

	/**
	* \brief Check if this GPU is Vulkan compatible
	* \param VkPhysicalDevice to inspect
	* \return true if the GPU supports Vulkan
	*/
	bool
		IsDeviceVulkanCompatible(
			const VkPhysicalDevice& physicalDeivce
			, const VkSurfaceKHR& surfaceKHR // For finding queue that can present image to our surface
		);

	// ===================
	// SWAPCHAIN
	// ===================

	Type::SwapchainSupport
		QuerySwapchainSupport(
			const VkPhysicalDevice& physicalDevice
			, const VkSurfaceKHR& surface
		);

	/**
	* \brief The surface format specifies color channel and types, and the texcoord space
	* \param availableFormats
	* \return the VkSurfaceFormatKHR that we desire.
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkSurfaceFormatKHR

	*/
	VkSurfaceFormatKHR
		SelectDesiredSwapchainSurfaceFormat(
			const std::vector<VkSurfaceFormatKHR> availableFormats
		);

	/**
	* \brief This is the most important setting for the swap chain. This is how we select
	*		  the condition when the image gets presented to the screen. There are four modes:
	*		  VK_PRESENT_MODE_IMMEDIATE_KHR:
	*				Image gets presented right away without vertical blanking. Tearing could occur.
	*				No internal queuing requests required.
	*
	*		  VK_PRESENT_MODE_MAILBOX_KHR:
	*				Waits for the next vertical blank. No tearing.
	*				A single-entry internal queue is used to hold the pending request.
	*				It doesn't block the application when the queue is full, instead
	*				new request replaces the old one inside the queue.
	*				One request per vertical blank interval.
	*				This can be used to implement tripple buffering with less latency and no tearing
	*				than the standard vertical sync with double buffering.
	*
	*		  VK_PRESENT_MODE_FIFO_KHR (guaranteed to be supported):
	*				Waits for the next vertical blank. No tearing.
	*				An internal queue is used to hold the pending requests.
	*				New requests are queued at the end. If the queue is full,
	*				this will block the application.
	*				One request per vertical blank interval.
	*				Most similar to vertical sync in modern games.
	*
	*
	*		  VK_PRESENT_MODE_FIFO_RELAXED_KHR:
	*				This generally waits for a vertical blank. But if one has occurs
	*				and the image arrives late, then it will present the next image right away.
	*				This mode also uses an internal queue to hold the pending requests.
	*				Use this when the images stutter.
	*				Tearing might be visible.
	*
	* \param availablePresentModes
	* \return the VkPresentModeKHR that we desire
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkPresentModeKHR
	*/
	VkPresentModeKHR
		SelectDesiredSwapchainPresentMode(
			const std::vector<VkPresentModeKHR> availablePresentModes
		);

	/**
	* \brief
	* \param surfaceCapabilities
	* \param useCurrentExtent: this should be true for most cases
	* \param desiredWidth: we can differ from the current extent if chosen so
	* \param desiredHeight: we can differ from the current extent if chosen so
	* \return
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkSurfaceCapabilitiesKHR
	*/
	VkExtent2D
		SelectDesiredSwapchainExtent(
			const VkSurfaceCapabilitiesKHR surfaceCapabilities
			, bool useCurrentExtent = true
			, unsigned int desiredWidth = 0 /* unused if useCurrentExtent is true */
			, unsigned int desiredHeight = 0 /* unused if useCurrentExtent is true */
		);

	// ===================
	// IMAGE
	// ===================

	/**
	* \brief Find a supported format from a list of candidates
	* \param physicalDevice
	* \param candidates
	* \param tiling
	* \param features
	* \return
	*/
	VkFormat
		FindSupportedFormat(
			const VkPhysicalDevice& physicalDevice,
			const std::vector<VkFormat>& candidates,
			VkImageTiling tiling,
			VkFormatFeatureFlags features
		);

	/**
	* \brief Find a supported depth format for a given physical device
	* \param physicalDevice
	* \return
	*/
	VkFormat
		FindDepthFormat(
			const VkPhysicalDevice& physicalDevice
		);

	/**
	* \brief Return true if the VkFormat contains a stencil component
	* \param format
	* \return
	*/
	bool
		DepthFormatHasStencilComponent(
			VkFormat format
		);
}
