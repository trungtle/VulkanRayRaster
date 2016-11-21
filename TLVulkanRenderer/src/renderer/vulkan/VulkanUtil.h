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
		VkVertexInputBindingDescription
		MakeVertexInputBindingDescription(
			uint32_t binding,
			uint32_t stride,
			VkVertexInputRate rate
		);

		VkVertexInputAttributeDescription
		MakeVertexInputAttributeDescription(
			uint32_t binding,
			uint32_t location,
			VkFormat format,
			uint32_t offset
		);

		VkPipelineVertexInputStateCreateInfo
		MakePipelineVertexInputStateCreateInfo(
			const std::vector<VkVertexInputBindingDescription>& bindingDesc,
			const std::vector<VkVertexInputAttributeDescription>& attribDesc
			);

		VkPipelineInputAssemblyStateCreateInfo
		MakePipelineInputAssemblyStateCreateInfo(
			VkPrimitiveTopology topology
		);

		VkViewport
		MakeFullscreenViewport(
			VkExtent2D extent
		);

		VkPipelineViewportStateCreateInfo
		MakePipelineViewportStateCreateInfo(
			const std::vector<VkViewport>& viewports,
			const std::vector<VkRect2D>& scissors
		);

		VkPipelineRasterizationStateCreateInfo
		MakePipelineRasterizationStateCreateInfo(
			VkPolygonMode polygonMode,
			VkCullModeFlags cullMode,
			VkFrontFace frontFace
		);

		VkPipelineMultisampleStateCreateInfo
		MakePipelineMultisampleStateCreateInfo(
			VkSampleCountFlagBits sampleCount
		);

		VkPipelineDepthStencilStateCreateInfo
		MakePipelineDepthStencilStateCreateInfo(
			VkBool32 depthTestEnable,
			VkBool32 depthWriteEnable,
			VkCompareOp compareOp
		);

		VkPipelineColorBlendStateCreateInfo
		MakePipelineColorBlendStateCreateInfo(
			const std::vector<VkPipelineColorBlendAttachmentState>& attachments
		);

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

		VkGraphicsPipelineCreateInfo
		MakeGraphicsPipelineCreateInfo(
			const std::vector<VkPipelineShaderStageCreateInfo>& shaderCreateInfos,
			const VkPipelineVertexInputStateCreateInfo* vertexInputStage,
			const VkPipelineInputAssemblyStateCreateInfo* inputAssemblyState,
			const VkPipelineTessellationStateCreateInfo* tessellationState,
			const VkPipelineViewportStateCreateInfo* viewportState,
			const VkPipelineRasterizationStateCreateInfo* rasterizationState,
			const VkPipelineColorBlendStateCreateInfo* colorBlendState,
			const VkPipelineMultisampleStateCreateInfo* multisampleState,
			const VkPipelineDepthStencilStateCreateInfo* depthStencilState,
			const VkPipelineDynamicStateCreateInfo* dynamicState,
			const VkPipelineLayout pipelineLayout,
			const VkRenderPass renderPass,
			const uint32_t subpass,
			const VkPipeline basePipelineHandle,
			const int32_t basePipelineIndex
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

	// -----------------------------------------------------

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
}
