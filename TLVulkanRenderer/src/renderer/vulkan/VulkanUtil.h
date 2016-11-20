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
}
