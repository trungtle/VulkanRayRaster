#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct Swapchain
{
	/**
	* \brief Abstraction for an array of images (VkImage) to be presented to the screen surface.
	*		  Typically, one image is presented at a time while multiple others can be queued.
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#_wsi_swapchain
	* \seealso VkImage
	*/

	VkSwapchainKHR swapchain;

	/**
	* \brief Array of images queued in the swapchain
	*/
	std::vector<VkImage> images;

	/**
	* \brief Image format inside swapchain
	*/
	VkFormat imageFormat;

	/**
	* \brief Image extent inside swapchain
	*/
	VkExtent2D extent;

	/**
	* \brief This is a view into the Vulkan
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#resources-image-views
	*/
	std::vector<VkImageView> imageViews;

	/**
	* \brief Swapchain framebuffers for each image view
	*/
	std::vector<VkFramebuffer> framebuffers;
};
