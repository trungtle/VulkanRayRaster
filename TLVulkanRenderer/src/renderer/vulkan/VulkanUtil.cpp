#include "VulkanUtil.h"
#include <algorithm>
#include <cassert>
#include <GLFW/glfw3.h>

namespace VulkanUtil
{

	bool
		CheckValidationLayerSupport(
			const std::vector<const char*>& validationLayers
		)
	{

		unsigned int layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availalbeLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availalbeLayers.data());

		// Find the layer
		for (auto layerName : validationLayers)
		{
			bool foundLayer = false;
			for (auto layerProperty : availalbeLayers)
			{
				foundLayer = (strcmp(layerName, layerProperty.layerName) == 0);
				if (foundLayer)
				{
					break;
				}
			}

			if (!foundLayer)
			{
				return false;
			}
		}
		return true;
	}

	std::vector<const char*>
		GetInstanceRequiredExtensions(
			bool enableValidationLayers
		)
	{
		std::vector<const char*> extensions;

		uint32_t extensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

		for (uint32_t i = 0; i < extensionCount; ++i)
		{
			extensions.push_back(glfwExtensions[i]);
		}

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}

	std::vector<const char*>
		GetDeviceRequiredExtensions(
			const VkPhysicalDevice& physicalDevice
		)
	{
		const std::vector<const char*> requiredExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

		for (auto& reqruiedExtension : requiredExtensions)
		{
			if (std::find_if(availableExtensions.begin(), availableExtensions.end(),
				[&reqruiedExtension](const VkExtensionProperties& prop)
			{
				return strcmp(prop.extensionName, reqruiedExtension) == 0;
			}) == availableExtensions.end())
			{
				// Couldn't find this extension, return an empty list
				return{};
			}
		}

		return requiredExtensions;
	}

	bool
	IsDeviceVulkanCompatible(
		const VkPhysicalDevice& physicalDeivce
		, const VkSurfaceKHR& surfaceKHR
	)
	{
		// Can this physical device support all the extensions we'll need (ex. swap chain)
		std::vector<const char*> requiredExtensions = GetDeviceRequiredExtensions(physicalDeivce);
		bool hasAllRequiredExtensions = requiredExtensions.size() > 0;

		// Check if we have the device properties desired
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDeivce, &deviceProperties);
		bool isDiscreteGPU = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

		// Query queue indices
		Type::QueueFamilyIndices queueFamilyIndices = FindQueueFamilyIndices(physicalDeivce, surfaceKHR);

		// Query swapchain support
		Type::SwapchainSupport swapchainSupport = QuerySwapchainSupport(physicalDeivce, surfaceKHR);

		return hasAllRequiredExtensions &&
			isDiscreteGPU &&
			swapchainSupport.IsComplete() &&
			queueFamilyIndices.IsComplete();
	}


	Type::QueueFamilyIndices
		FindQueueFamilyIndices(
			const VkPhysicalDevice& physicalDeivce
			, const VkSurfaceKHR& surfaceKHR
		)
	{
		Type::QueueFamilyIndices queueFamilyIndices;

		uint32_t queueFamilyPropertyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDeivce, &queueFamilyPropertyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDeivce, &queueFamilyPropertyCount, queueFamilyProperties.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilyProperties)
		{
			// We need at least one graphics queue
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queueFamilyIndices.graphicsFamily = i;
			}

			// We need at least one queue that can present image to the KHR surface.
			// This could be a different queue from our graphics queue.
			// @todo: enforce graphics queue and present queue to be the same?
			VkBool32 presentationSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDeivce, i, surfaceKHR, &presentationSupport);

			if (queueFamily.queueCount > 0 && presentationSupport)
			{
				queueFamilyIndices.presentFamily = i;
			}

			// Query compute queue family
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				queueFamilyIndices.computeFamily = i;
			}

			// Query memory transfer family
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				queueFamilyIndices.transferFamily = i;
			}

			if (queueFamilyIndices.IsComplete())
			{
				break;
			}

			++i;
		}

		return queueFamilyIndices;
	}

	Type::SwapchainSupport
		QuerySwapchainSupport(
			const VkPhysicalDevice& physicalDevice
			, const VkSurfaceKHR& surface
		)
	{
		Type::SwapchainSupport swapchainInfo;

		// Query surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &swapchainInfo.capabilities);

		// Query supported surface formats
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			swapchainInfo.surfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, swapchainInfo.surfaceFormats.data());
		}

		// Query supported surface present modes
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			swapchainInfo.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, swapchainInfo.presentModes.data());
		}

		return swapchainInfo;
	}

	VkSurfaceFormatKHR
		SelectDesiredSwapchainSurfaceFormat(
			const std::vector<VkSurfaceFormatKHR> availableFormats
		)
	{
		assert(availableFormats.empty() == false);

		// List of some formats options we would like to choose from. Rank from most preferred down.
		// @todo: move this to a configuration file so we could easily configure it in the future
		std::vector<VkSurfaceFormatKHR> preferredFormats = {

			// *N.B*
			// We want to use sRGB to display to the screen, since that's the color space our eyes can perceive accurately.
			// See http://stackoverflow.com/questions/12524623/what-are-the-practical-differences-when-working-with-colors-in-a-linear-vs-a-no
			// for more details.
			// For mannipulating colors, use a 32 bit unsigned normalized RGB since it's an easier format to do math with.

			{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
		};

		// Just return the first reasonable format we found
		for (const auto& format : availableFormats)
		{
			for (const auto& preferred : preferredFormats)
			{
				if (format.format == preferred.format && format.colorSpace == preferred.colorSpace)
				{
					return format;
				}
			}
		}

		// Couldn't find one that satisfy our preferrence, so just return the first one we found
		return availableFormats[0];
	}

	VkPresentModeKHR
		SelectDesiredSwapchainPresentMode(
			const std::vector<VkPresentModeKHR> availablePresentModes
		)
	{
		assert(availablePresentModes.empty() == false);

		// Maybe we should do tripple buffering here to avoid tearing & stuttering
		// @todo: This SHOULD be a configuration passed from somewhere else in a global configuration state
		bool enableTrippleBuffering = true;
		if (enableTrippleBuffering)
		{
			// Search for VK_PRESENT_MODE_MAILBOX_KHR. This is because we're interested in 
			// using tripple buffering.
			for (const auto& presentMode : availablePresentModes)
			{
				if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					return VK_PRESENT_MODE_MAILBOX_KHR;
				}
			}
		}

		// Couldn't find one that satisfy our preferrence, so just return
		// VK_PRESENT_MODE_FIFO_KHR, since it is guaranteed to be supported by the Vulkan spec
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D
		SelectDesiredSwapchainExtent(
			const VkSurfaceCapabilitiesKHR surfaceCapabilities
			, bool useCurrentExtent
			, unsigned int desiredWidth
			, unsigned int desiredHeight
		)
	{
		// @ref From Vulkan 1.0.29 spec (with KHR extension) at
		// https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#VkSurfaceCapabilitiesKHR
		// "currentExtent is the current width and height of the surface, or the special value (0xFFFFFFFF, 0xFFFFFFFF) 
		// indicating that the surface size will be determined by the extent of a swapchain targeting the surface."
		// So we first check if this special value is set. If it is, proceed with the desiredWidth and desiredHeight
		if (surfaceCapabilities.currentExtent.width != 0xFFFFFFFF ||
			surfaceCapabilities.currentExtent.height != 0xFFFFFFFF)
		{
			return surfaceCapabilities.currentExtent;
		}

		// Pick the extent that user prefer here
		VkExtent2D extent;

		// Properly select extent based on our surface capability's min and max of the extent
		uint32_t minWidth = surfaceCapabilities.minImageExtent.width;
		uint32_t maxWidth = surfaceCapabilities.maxImageExtent.width;
		uint32_t minHeight = surfaceCapabilities.minImageExtent.height;
		uint32_t maxHeight = surfaceCapabilities.maxImageExtent.height;
		extent.width = std::max(minWidth, std::min(maxWidth, static_cast<uint32_t>(desiredWidth)));
		extent.height = std::max(minHeight, std::min(maxHeight, static_cast<uint32_t>(desiredHeight)));

		return extent;
	}

	VkFormat
		FindSupportedFormat(
			const VkPhysicalDevice& physicalDevice,
			const std::vector<VkFormat>& candidates,
			VkImageTiling tiling,
			VkFormatFeatureFlags features
		)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR &&
				(props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
				(props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		throw std::runtime_error("Failed to find a supported format");
	}

	VkFormat
		FindDepthFormat(
			const VkPhysicalDevice& physicalDevice
		)
	{
		return FindSupportedFormat(
			physicalDevice,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool
		DepthFormatHasStencilComponent(
			VkFormat format
		)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}


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
