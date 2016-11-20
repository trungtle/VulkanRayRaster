#pragma once

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanSwapchain.h"
#include "VulkanUtil.h"
#include <GLFW/glfw3.h>
#include <spdlog/logger.h>

class VulkanDevice {

public:

	VulkanDevice(
		GLFWwindow* window,
		std::string name, 
		std::shared_ptr<spdlog::logger> logger
			) :
		isEnableValidationLayers(true),
		debugCallback(nullptr),
		instance(nullptr), 
		surfaceKHR(nullptr), 
		physicalDevice(nullptr), 
		device(nullptr), 
		m_name(name), 
		m_logger(logger) 
	{
		Initialize(window);
	};

	~VulkanDevice() 
	{
		Destroy();
	}

	/**
	* \brief If true, will include the validation layer
	*/
	bool isEnableValidationLayers;
	
	/**
	* \brief This is the callback for the debug report in the Vulkan validation extension
	*/
	VkDebugReportCallbackEXT debugCallback;

	/**
	* \brief Handle to the per-application Vulkan instance.
	*		 There is no global state in Vulkan, and the instance represents per-application state.
	*		 Creating an instance also initializes the Vulkan library.
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#initialization-instances
	*/
	VkInstance instance;

	/**
	* \brief Abstract for native platform surface or window object
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#_wsi_surface
	*/
	VkSurfaceKHR surfaceKHR;

	/**
	* \brief Handle to the actual GPU, or physical device. It is used for informational purposes only and maynot affect the Vulkan state itself.
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#devsandqueues-physical-device-enumeration
	*/
	VkPhysicalDevice physicalDevice;

	/**
	* \brief In Vulkan, this is called a logical device. It represents the logical connection
	*		  to physical device and how the application sees it.
	* \ref https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#devsandqueues-devices
	*/
	VkDevice device;

	Swapchain m_swapchain;


	/**
	* \brief Store the queue family indices
	*/
	VulkanUtil::Type::QueueFamilyIndices queueFamilyIndices;

	void Initialize(GLFWwindow* window);

	void Destroy();


private:
	/**
	* \brief Name of the Vulkan application. This is the name of our whole application in general.
	*/
	std::string m_name;

	std::shared_ptr<spdlog::logger> m_logger;


	VkResult
		InitializeVulkanInstance();

	VkResult
		SetupDebugCallback();


	VkResult
		CreateWindowSurface(GLFWwindow* window);

	VkResult
		SelectPhysicalDevice();

	VkResult
		SetupLogicalDevice();

	VkResult
		PrepareSwapchain();

};
