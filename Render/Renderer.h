#pragma once

#include"allincludes.h"
#include"Window.h"
#include"Platform.h"
#include"BUILD_OPTIONS.h"
#include"Shared.h"

class Window;

class Renderer
{
public:
	Renderer();
	~Renderer();

	Window* OpenWindow(uint32_t size_x, uint32_t size_y, std::string name);

	bool   Run();

	const VkInstance                          GetVulkanInstance() const;
	const VkPhysicalDevice                    GetVulkanPhysicalDevice() const; 
	const VkDevice                            GetVulkanDevice() const; 
	const VkQueue                             GetVulkanQueue() const;
	const uint32_t                            GetVulkanGraphicsQueueFamilyIndex() const;
	const VkPhysicalDeviceProperties       &  GetVulkanPhysicalDeviceProperties() const;
	const VkPhysicalDeviceMemoryProperties &  GetVulkanPhysicalDeviceMemoryProperties() const;
	const VkDebugReportCallbackEXT            GetVulkanDebugReportCallback() const;
	const VkSampleCountFlagBits               GetVulkanMsaa() const;

private:
	void _SetupLayersAndExtentions();

	void _InitInstance();
	void _DeInitInstance();

	void _InitDevice();
	void _DeInitDevice();

	void _SetupDebug();
	void _InitDebug();
	void _DeInitDebug();

	VkSampleCountFlagBits getMaxUsableSampleCount();
	bool isDeviceSuitable(VkPhysicalDevice device);

	VkInstance                        _instance      = VK_NULL_HANDLE;
	VkPhysicalDevice                  _gpu           = VK_NULL_HANDLE;
	VkDevice                          _device        = VK_NULL_HANDLE;
	VkQueue                           _queue         = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties        _gpu_propertie = {};
	VkPhysicalDeviceMemoryProperties  _gpu_memory_propertie = {};
	VkPhysicalDeviceFeatures          supported_physical_device_feature = {};


	uint32_t          _graphics_family_index = 0;

	Window*           _window     = nullptr;

	std::vector<const char*> _instance_layers;
	std::vector<const char*> _instance_extentions;
	std::vector<const char*> _device_extentions;


	VkDebugReportCallbackEXT  _debug_report = VK_NULL_HANDLE;
	VkDebugReportCallbackCreateInfoEXT debug_callback_create_info = {};

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
};

