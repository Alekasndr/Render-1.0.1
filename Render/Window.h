#pragma once

#include"Platform.h"
#include<iostream>
#include<vector>

using namespace std;

class Renderer;

class Window
{
public:
	Window(Renderer * renderer, uint32_t size_x, uint32_t size_y, std::string name);
	~Window();

	void Close();
	bool Update();

	void BeginRender();
	void EndRender( std::vector<VkSemaphore> wait_semaphore);

	VkRenderPass GetVulkanRenderPass();
	VkFramebuffer GetVulkanFramebuffer();
	VkExtent2D GetVulkanSurfaceSize();
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

private:

	void  _InitOSWindow();
	void  _DeInitOSWindow();
	void  _UpdateOSWindow();
	void  _InitOSSurface();

	void  _InitSurface();
	void  _DenitSurface();

	void  _InitSwapchain();
	void  _DeinitSwapchain();

	void  _InitSwapchainImages();
	void  _DeInitSwapchainImages();

	void  _InitDepthStencilImage();
	void  _DeInitDepthStencilImage();

	void  _InitRenderPass();
	void  _DeInitRednderPass();

	void  _InitFramebuffers();
	void  _DeInitFramebuffers();

	void  _InitSynchronization();
	void  _DeInitSynchronization();
	 
	void  _CreateGraphicsPipeline();
	void  _DestroyGraphicsPipeline();
	
	void _CreateCommandPool();
	void _DestroyCommandPool();
	
	Renderer   * _renderer = nullptr;

	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;

	VkSurfaceKHR   _surface = VK_NULL_HANDLE;

	uint32_t _surface_size_x = 512;
	uint32_t _surface_size_y = 512;
	std::string _window_name;
	uint32_t   _swapchain_image_count = 2;
	uint32_t _active_swapchain_image_id = UINT32_MAX;

	VkFence _swapchain_image_available = VK_NULL_HANDLE;

	VkSurfaceFormatKHR _surface_format = {};
	VkSurfaceCapabilitiesKHR _surface_capabilities = {};
	VkRenderPass _render_pass = VK_NULL_HANDLE;

	VkFormat _depth_stencil_format = VK_FORMAT_UNDEFINED;
	bool _stencil_avalible = false;

	std::vector<VkImage>  _swapchain_images;
	std::vector<VkImageView> _swapchain_images_views;
	std::vector<VkFramebuffer> _framebuffer;

	VkImage  _depth_stencil_image = VK_NULL_HANDLE;
	VkDeviceMemory  _depth_stencil_image_memory = VK_NULL_HANDLE;
	VkImageView _depth_stencil_image_view = VK_NULL_HANDLE;

	VkShaderModule _shaderModule = VK_NULL_HANDLE;
	
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

	VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

	VkCommandPool _commandPool = VK_NULL_HANDLE;

	bool _window_should_run = true;

#if VK_USE_PLATFORM_WIN32_KHR
	HINSTANCE         _win32_instance = NULL;
	HWND              _win32_window   = NULL;
	std::string       _win32_class_name;
	static uint64_t   _win32_class_id_counter;
#endif
};
