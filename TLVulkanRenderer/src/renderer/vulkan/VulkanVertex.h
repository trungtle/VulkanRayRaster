#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include "SceneUtil.h"

namespace VulkanVertex {
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
	GetVertexInputAttributeDescriptions()
	{
		VertexAttributeDescriptions attributeDesc;

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

