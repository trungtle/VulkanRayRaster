#include "VulkanUtil.h"
#include <algorithm>
#include <cassert>
#include <GLFW/glfw3.h>
#include "VulkanSwapchain.h"

namespace VulkanUtil
{
	namespace Make
	{
		VkDescriptorPoolSize
			MakeDescriptorPoolSize(
				VkDescriptorType type,
				uint32_t descriptorCount
			)
		{
			VkDescriptorPoolSize poolSize = {};
			poolSize.type = type;
			poolSize.descriptorCount = descriptorCount;

			return poolSize;
		}

		VkDescriptorPoolCreateInfo
			MakeDescriptorPoolCreateInfo(
				uint32_t poolSizeCount,
				VkDescriptorPoolSize* poolSizes,
				uint32_t maxSets
			)
		{
			VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
			descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolCreateInfo.poolSizeCount = poolSizeCount;
			descriptorPoolCreateInfo.pPoolSizes = poolSizes;
			descriptorPoolCreateInfo.maxSets = maxSets;

			return descriptorPoolCreateInfo;
		}

		VkDescriptorSetLayoutBinding
			MakeDescriptorSetLayoutBinding(
				uint32_t binding,
				VkDescriptorType descriptorType,
				VkShaderStageFlags shaderFlags,
				uint32_t descriptorCount
			)
		{
			VkDescriptorSetLayoutBinding layoutBinding = {};
			layoutBinding.binding = binding;
			layoutBinding.descriptorType = descriptorType;
			layoutBinding.stageFlags = shaderFlags;
			layoutBinding.descriptorCount = descriptorCount;

			return layoutBinding;
		}

		VkDescriptorSetLayoutCreateInfo
			MakeDescriptorSetLayoutCreateInfo(
				VkDescriptorSetLayoutBinding* bindings,
				uint32_t bindingCount
			)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = {};

			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = bindingCount;
			layoutInfo.pBindings = bindings;

			return layoutInfo;
		}

		VkDescriptorSetAllocateInfo
			MakeDescriptorSetAllocateInfo(
				VkDescriptorPool descriptorPool,
				VkDescriptorSetLayout* descriptorSetLayout,
				uint32_t descriptorSetCount
			)
		{

			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = descriptorSetLayout;

			return allocInfo;

		}

		VkDescriptorBufferInfo
			MakeDescriptorBufferInfo(
				VkBuffer buffer,
				VkDeviceSize offset,
				VkDeviceSize range
			)
		{
			VkDescriptorBufferInfo descriptorBufferInfo = {};
			descriptorBufferInfo.buffer = buffer;
			descriptorBufferInfo.offset = offset;
			descriptorBufferInfo.range = range;

			return descriptorBufferInfo;
		}

		VkWriteDescriptorSet
			MakeWriteDescriptorSet(
				VkDescriptorType type,
				VkDescriptorSet dstSet,
				uint32_t dstBinding,
				uint32_t descriptorCount,
				VkDescriptorBufferInfo* bufferInfo,
				VkDescriptorImageInfo* imageInfo
			)
		{
			VkWriteDescriptorSet writeDescriptorSet = {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.dstBinding = dstBinding;
			writeDescriptorSet.dstArrayElement = 0; // descriptor set could be an array
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.descriptorCount = descriptorCount;
			writeDescriptorSet.pBufferInfo = bufferInfo;
			writeDescriptorSet.pImageInfo = imageInfo;

			return writeDescriptorSet;
		}

		VkPipelineLayoutCreateInfo
			MakePipelineLayoutCreateInfo(
				VkDescriptorSetLayout* descriptorSetLayouts,
				uint32_t setLayoutCount
			)
		{
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
			pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
			return pipelineLayoutCreateInfo;
		}

		VkComputePipelineCreateInfo
			MakeComputePipelineCreateInfo(VkPipelineLayout layout, VkPipelineCreateFlags flags) {
			VkComputePipelineCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			createInfo.layout = layout;
			createInfo.flags = flags;

			return createInfo;
		}

		VkPipelineShaderStageCreateInfo
			MakePipelineShaderStageCreateInfo(
				VkShaderStageFlagBits stage,
				const VkShaderModule& shaderModule
			)
		{
			VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
			shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageCreateInfo.stage = stage;
			shaderStageCreateInfo.module = shaderModule;
			// Specify entry point. It's possible to combine multiple shaders into a single shader module
			shaderStageCreateInfo.pName = "main";

			// This can be used to set values for shader constants. The compiler can perform optimization for these constants vs. if they're created as variables in the shaders.
			shaderStageCreateInfo.pSpecializationInfo = nullptr;

			return shaderStageCreateInfo;
		}

		void
			MakeDefaultTextureSampler(
				const VkDevice& device,
				VkSampler* sampler
			)
		{
			VkSamplerCreateInfo samplerCreateInfo = {};
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.maxAnisotropy = 0;
			CheckVulkanResult(
				vkCreateSampler(device, &samplerCreateInfo, nullptr, sampler),
				"Failed to create texture sampler"
			);
		}

		VkCommandPoolCreateInfo
			MakeCommandPoolCreateInfo(
				uint32_t queueFamilyIndex
			)
		{
			VkCommandPoolCreateInfo commandPoolCreateInfo = {};
			commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

			// From Vulkan spec:
			// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT indicates that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe. This flag may be used by the implementation to control memory allocation behavior within the pool.
			// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT controls whether command buffers allocated from the pool can be individually reset.If this flag is set, individual command buffers allocated from the pool can be reset either explicitly, by calling vkResetCommandBuffer, or implicitly, by calling vkBeginCommandBuffer on an executable command buffer.If this flag is not set, then vkResetCommandBuffer and vkBeginCommandBuffer(on an executable command buffer) must not be called on the command buffers allocated from the pool, and they can only be reset in bulk by calling vkResetCommandPool.
			commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

			return commandPoolCreateInfo;
		}

		VkCommandBufferAllocateInfo
			MakeCommandBufferAllocateInfo(
				VkCommandPool commandPool,
				VkCommandBufferLevel level,
				uint32_t bufferCount
			)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = commandPool;
			// Primary means that can be submitted to a queue, but cannot be called from other command buffers
			allocInfo.level = level;
			allocInfo.commandBufferCount = bufferCount;

			return allocInfo;
		}
	}
}
