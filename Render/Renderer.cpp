#include "Renderer.h"

Renderer::Renderer()
{
	_SetupLayersAndExtentions();
	_SetupDebug();
	_InitInstance();
	_InitDebug();
	_InitDevice();
}

Renderer::~Renderer()
{
	delete _window;
	_DeInitDevice();
	_DeInitDebug();
	_DeInitInstance();
}

Window* Renderer::OpenWindow(uint32_t size_x, uint32_t size_y, std::string name, GltfLoader* gltf)
{
	_window = new Window(this, size_x, size_y, name, gltf);
	return _window;
}

bool Renderer::Run()
{
	if (nullptr != _window) {
		return _window->Update();
	}
	return true;
}

const VkInstance Renderer::GetVulkanInstance() const
{
	return _instance;
}

const VkPhysicalDevice Renderer::GetVulkanPhysicalDevice() const
{
	return _gpu;
}

const VkDevice Renderer::GetVulkanDevice() const
{
	return _device;
}

const VkQueue Renderer::GetVulkanQueue() const
{
	return _queue;
}

const uint32_t Renderer::GetVulkanGraphicsQueueFamilyIndex() const
{
	return _graphics_family_index;
}

const VkPhysicalDeviceProperties& Renderer::GetVulkanPhysicalDeviceProperties() const
{
	return _gpu_propertie;
}

const VkPhysicalDeviceMemoryProperties& Renderer::GetVulkanPhysicalDeviceMemoryProperties() const
{
	return _gpu_memory_propertie;
}

const VkDebugReportCallbackEXT Renderer::GetVulkanDebugReportCallback() const
{
	return _debug_report;
}

const VkSampleCountFlagBits Renderer::GetVulkanMsaa() const
{
	return msaaSamples;
}


void Renderer::_SetupLayersAndExtentions() {
//	_instance_extentions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
	_instance_extentions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	_instance_extentions.push_back(PLATFORM_SURFACE_EXTENSION_NAME);
	_device_extentions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	/*
	uint32_t numInstanceExtension = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExtension, nullptr);
	if (numInstanceExtension != 0) {
		std::vector<VkExtensionProperties> _supported_instance_extensions(numInstanceExtension);
		vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExtension, _supported_instance_extensions.data());
		std::cout << "Instance Extension: \n";
		for (auto& i : _supported_instance_extensions) {
			std::cout << "  " << i.extensionName << std::endl;
		}
		std::cout << std::endl;
	}
	*/
}

void Renderer::_InitInstance()
{
	VkApplicationInfo application_info{};
	application_info.sType                    = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.apiVersion			      = VK_MAKE_VERSION(1, 2, 154);
	application_info.applicationVersion       = VK_MAKE_VERSION(1, 1, 2);
	application_info.pApplicationName         = "Vulkan Renderer 1.0.2";
	application_info.pNext = NULL;

	VkInstanceCreateInfo instance_create_info{};
	instance_create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pApplicationInfo        = &application_info;
	instance_create_info.enabledLayerCount       = _instance_layers.size();
	instance_create_info.ppEnabledLayerNames     = _instance_layers.data();
	instance_create_info.enabledExtensionCount   = _instance_extentions.size();
	instance_create_info.ppEnabledExtensionNames = _instance_extentions.data();
	instance_create_info.pNext                   = NULL;

	ErrorCheck (vkCreateInstance( &instance_create_info, nullptr, &_instance));
	std::cout << "Vulkan: Instance sucessfully created" << std::endl;
}

void Renderer::_DeInitInstance()
{
	vkDestroyInstance(_instance, nullptr);
	_instance = nullptr;
	std::cout << "Vulkan: Instance sucessfully destroyed" << std::endl;
}

void Renderer::_InitDevice()
{
	{
		uint32_t gpu_count = 0;
		vkEnumeratePhysicalDevices(_instance, &gpu_count, nullptr);
		std::vector<VkPhysicalDevice> gpu_list(gpu_count);
		vkEnumeratePhysicalDevices(_instance, &gpu_count, gpu_list.data());

		for (const auto& device : gpu_list) {
			if (isDeviceSuitable(device)) {
				_gpu = device;
				msaaSamples = getMaxUsableSampleCount();
				break;
			}
		}

		vkGetPhysicalDeviceProperties(_gpu, &_gpu_propertie);
		vkGetPhysicalDeviceMemoryProperties(_gpu, &_gpu_memory_propertie);
		vkGetPhysicalDeviceFeatures(_gpu, &supported_physical_device_feature);
		supported_physical_device_feature.samplerAnisotropy = VK_TRUE;
		supported_physical_device_feature.sampleRateShading = VK_TRUE;
	}
	{
		uint32_t family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(_gpu, &family_count, nullptr);
		std::vector < VkQueueFamilyProperties> familu_property_list(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(_gpu, &family_count, familu_property_list.data());
	
		

		bool found = false;
		for (uint32_t i = 0; i < family_count; ++i) {
			if (familu_property_list[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				found = true;
				_graphics_family_index = i;
			}
		}
		if (!found) {
			std::cout << "Vulkan ERROR: Queue family supporting graphics not found." << std::endl;
			assert(0 && "Vulkan ERROR: Queue family supporting graphics not found.");
			std::exit(-1);
		}

	}

	/*
	{
		uint32_t layer_count = 0;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
		std::vector<VkLayerProperties> layer_properties_list(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, layer_properties_list.data());
		std::cout << "Instance Layers: \n";
		for (auto &i : layer_properties_list) {
			std::cout << "  " << i.layerName << "\t\t |" << i.description << std::endl;
		}
		std::cout << std::endl;
	}
	*/

	float queue_priorities[]{ 1.0f };
	VkDeviceQueueCreateInfo device_queue_create_info{};
	device_queue_create_info.sType                 = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	device_queue_create_info.pNext = VK_NULL_HANDLE;
	device_queue_create_info.queueFamilyIndex      = _graphics_family_index;
	device_queue_create_info.queueCount            = 1;
	device_queue_create_info.pQueuePriorities      = queue_priorities;
	device_queue_create_info.pNext = NULL;

	VkDeviceCreateInfo device_create_info{}; 
	device_create_info.sType                        = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_queue_create_info.pNext = VK_NULL_HANDLE;
	device_create_info.queueCreateInfoCount         = 1;
	device_create_info.pQueueCreateInfos            = &device_queue_create_info;
	device_create_info.enabledExtensionCount = _device_extentions.size();
	device_create_info.ppEnabledExtensionNames = _device_extentions.data();
	device_create_info.pEnabledFeatures = &supported_physical_device_feature;
	device_create_info.pNext = NULL;

	

	ErrorCheck(vkCreateDevice(_gpu ,&device_create_info, nullptr, &_device));
	
	vkGetDeviceQueue(_device, _graphics_family_index, 0, &_queue);
//	vkGetDeviceQueue(_device, _graphics_family_index, 1, &_queue);
	
	std::cout << "Vulkan: Device successfully initialized "
		<< std::endl;
} 

void Renderer::_DeInitDevice()
{
	vkDestroyDevice(_device, nullptr);
	_device = nullptr;
	std::cout << "Vulkan: Device successfully destroyed" << std::endl;
}

#if BUILD_ENABLE_VULKAN_DEBUG

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  obj_type,
	uint64_t                    scr_obj,
	size_t                      location,
	int32_t                     msg_code,
	const char* layer_prefix,
	const char* msg,
	void* user_data)
{
	std::cout <<  flags << std::endl;
	std::ostringstream stream;
	stream << "VKDBG:";
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
		stream << "INFO:";
	}
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
		stream << "WARNING:";
	}
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
		stream << "PERFOMANCE:";
	}
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		stream << "ERROR:";
	}
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
		stream << "DEBUG:";
	}

	stream << "@[" << layer_prefix << "]:";
	stream << msg << std::endl;
	
	std::cout << stream.str();

#ifdef _WIN32
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
		MessageBoxA(NULL, stream.str().c_str(), "Vulkan Error!", 0);
	}
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		MessageBoxA(NULL, stream.str().c_str(), "Vulkan Error!", 0);
	}
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
		MessageBoxA(NULL, stream.str().c_str(), "Vulkan Error!", 0);
	}
#endif // _WIN32

	return false;
}

void Renderer::_SetupDebug()
{
	debug_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debug_callback_create_info.pNext = NULL;
	debug_callback_create_info.pfnCallback = VulkanDebugCallback;
	debug_callback_create_info.flags =
//		VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_ERROR_BIT_EXT |
//		VK_DEBUG_REPORT_DEBUG_BIT_EXT |
		0;


	_instance_layers.push_back("VK_LAYER_KHRONOS_validation");

	_instance_extentions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
}

PFN_vkCreateDebugReportCallbackEXT   fvkCreateDebugReportCallbackEXT    = nullptr;
PFN_vkDestroyDebugReportCallbackEXT   fvkDestroyDebugReportCallbackEXT  = nullptr;


void Renderer::_InitDebug()
{
	fvkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugReportCallbackEXT");
	fvkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");

	if (nullptr == fvkCreateDebugReportCallbackEXT || nullptr == fvkDestroyDebugReportCallbackEXT) {
		std::cout << "Vulkan ERROR: Can't fetch debug function pointers." << std::endl;
		assert(0 && "Vulkan ERROR: Can't fetch debug function pointers.");
		std::exit(-1);
	}
	std::cout << "Vulkan: Fetch debug function pointers initialized "
		<< std::endl;

	


	fvkCreateDebugReportCallbackEXT(_instance, &debug_callback_create_info,nullptr, &_debug_report);
}

void Renderer::_DeInitDebug()
{
	fvkDestroyDebugReportCallbackEXT(_instance, _debug_report, nullptr);
	_debug_report = VK_NULL_HANDLE;
	std::cout << "Vulkan: Debug report destroyed" << std::endl;
}

VkSampleCountFlagBits Renderer::getMaxUsableSampleCount()
{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(_gpu, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device)
{
	return true;
}


#else 
void Renderer::_SetupDebug() {};
void Renderer::_InitDebug() {};
void Renderer::_DeInitDebug() {};

#endif //BUILD_ENABLE_VULKAN_DEBUG
