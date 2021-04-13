#include "Window.h"
#include"Renderer.h"
#include"Shared.h"
#include<assert.h>
#include<array>
#include<fstream>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

Window::Window(Renderer * renderer, uint32_t size_x, uint32_t size_y, std::string name)
{
	_renderer       = renderer;
	_surface_size_x = size_x;
	_surface_size_y = size_y;
	_window_name    = name;

	_InitOSWindow();
	_InitSurface();
	_InitSwapchain();
	_InitSwapchainImages();
	_InitDepthStencilImage();
	_InitRenderPass();
	createDescriptorSetLayout();
	_CreateGraphicsPipeline();
	_InitFramebuffers();
	_CreateCommandPool();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	_CreateCommandBuffers();
	createSyncObjects();
}

Window::~Window()
{
	vkQueueWaitIdle(_renderer->GetVulkanQueue());

	destroySyncObjects();
	_DestroyCommandBuffers();
	destroyDescriptorPool();
	destroyUniformBuffers();
	destroyIndexBuffer();
	destroyVertexBuffer();
	_DestroyCommandPool();
	_DeInitFramebuffers();
	_DestroyGraphicsPipeline();
	destroyDescriptorSetLayout();
	_DeInitRednderPass();
	_DeInitDepthStencilImage();
	_DeInitSwapchainImages();
	_DeinitSwapchain();
	_DenitSurface();
	_DeInitOSWindow();
}

void Window::Close()
{
	_window_should_run = false;
}

bool Window::Update()
{
	_UpdateOSWindow();
	return _window_should_run;
}

void Window::DrawFrame()
{
	auto device = _renderer->GetVulkanDevice();
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &inFlightFences[currentFrame]);
	uint32_t imageIndex;

	VkResult result = vkAcquireNextImageKHR(device, 
		_swapchain, UINT64_MAX, 
		imageAvailableSemaphores[currentFrame],
		VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Vulkan: Failed to acquire swap chain image!");
	}

	// Check if a previous frame is using this image (i.e. there is its fence to wait on)
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}
	// Mark the image as now being in use by this frame
	imagesInFlight[imageIndex] = inFlightFences[currentFrame];

	updateUniformBuffer(imageIndex);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &inFlightFences[currentFrame]);
	if (vkQueueSubmit(_renderer->GetVulkanQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to submit draw command buffer!");
	}

	VkSwapchainKHR swapChains[] = { _swapchain };

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = &result;

	ErrorCheck(result = vkQueuePresentKHR(_renderer->GetVulkanQueue(), &presentInfo));

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to present swap chain image!");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	ErrorCheck(vkQueueWaitIdle(_renderer->GetVulkanQueue()));
}

std::vector<VkCommandBuffer> Window::GetVulkanCommandBuffer()
{
	return _commandBuffers;
}

VkRenderPass Window::GetVulkanRenderPass()
{
	return _render_pass;
}

VkFramebuffer Window::GetVulkanFramebuffer()
{
	return _framebuffer[_active_swapchain_image_id];
}

VkExtent2D Window::GetVulkanSurfaceSize()
{
	return { _surface_size_x, _surface_size_y };
}


void Window::_InitSurface()
{
	_InitOSSurface();

	auto gpu = _renderer->GetVulkanPhysicalDevice();

	VkBool32 WSI_supported = false;
	vkGetPhysicalDeviceSurfaceSupportKHR(gpu, _renderer->GetVulkanGraphicsQueueFamilyIndex(), _surface, &WSI_supported);
	if (!WSI_supported) {
		assert(0 && "WSI not supported");
		std::exit(-1);
	}

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, _surface, &_surface_capabilities);

	if (_surface_capabilities.currentExtent.width < UINT32_MAX) {
		_surface_size_x = _surface_capabilities.currentExtent.width;
		_surface_size_y = _surface_capabilities.currentExtent.height;
	}

	{
		uint32_t format_count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, _surface, &format_count, nullptr);
		if (format_count == 0) {
			assert(0 && "Surface formats missing");
			std::exit(-1);
		}
		std::vector<VkSurfaceFormatKHR> formats(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, _surface, &format_count, formats.data());
		if (formats[0].format == VK_FORMAT_UNDEFINED) {
			_surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
			_surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		}
		else{
			_surface_format = formats[0];
		}
	}
}

void Window::_DenitSurface()
{
	vkDestroySurfaceKHR(_renderer->GetVulkanInstance(), _surface, nullptr);
}

void Window::_InitSwapchain()
{
	auto device = _renderer->GetVulkanDevice();

	if (_swapchain_image_count < _surface_capabilities.minImageCount  + 1) _swapchain_image_count = _surface_capabilities.minImageCount + 1;
	if (_surface_capabilities.maxImageCount > 0) {
		if (_swapchain_image_count > _surface_capabilities.maxImageCount) _swapchain_image_count = _surface_capabilities.maxImageCount;
	}

	/// <summary>
	/// ////////////////
	/// </summary>

	VkPresentModeKHR persent_mode = VK_PRESENT_MODE_FIFO_KHR;
	{
		uint32_t present_mode_count = 0;
		ErrorCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(_renderer->GetVulkanPhysicalDevice(), _surface, &present_mode_count,nullptr));
		std::vector<VkPresentModeKHR>present_mode_list(present_mode_count);
		ErrorCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(_renderer->GetVulkanPhysicalDevice(), _surface, &present_mode_count, present_mode_list.data()));
		for (auto m : present_mode_list) {
			if (m == VK_PRESENT_MODE_MAILBOX_KHR) persent_mode = m;
		}
	}

	/// <summary>
	/// ////////////
	/// </summary>

	VkSwapchainCreateInfoKHR swapchain_creater_info{};
	swapchain_creater_info.sType               = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_creater_info.surface             = _surface;
	swapchain_creater_info.minImageCount       = _swapchain_image_count;
	swapchain_creater_info.imageFormat         = _surface_format.format;
	swapchain_creater_info.imageColorSpace     = _surface_format.colorSpace;
	swapchain_creater_info.imageExtent.width   = _surface_size_x;
	swapchain_creater_info.imageExtent.height  = _surface_size_y;
	swapchain_creater_info.imageArrayLayers    = 1;
	swapchain_creater_info.imageUsage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_creater_info.imageSharingMode    = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_creater_info.queueFamilyIndexCount = 0;
	swapchain_creater_info.pQueueFamilyIndices = nullptr;
	swapchain_creater_info.preTransform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_creater_info.compositeAlpha      = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_creater_info.presentMode         = persent_mode;
	swapchain_creater_info.clipped             = VK_TRUE;
	swapchain_creater_info.oldSwapchain        = VK_NULL_HANDLE;

	ErrorCheck(vkCreateSwapchainKHR(device, &swapchain_creater_info, nullptr, &_swapchain));

	ErrorCheck(vkGetSwapchainImagesKHR(device, _swapchain, &_swapchain_image_count, nullptr));
}

void Window::_DeinitSwapchain()
{
	auto device = _renderer->GetVulkanDevice();
	vkDestroySwapchainKHR(device, _swapchain, nullptr);
}

void Window::_InitSwapchainImages()
{
	auto device = _renderer->GetVulkanDevice();

	_swapchain_images.resize(_swapchain_image_count);
	_swapchain_images_views.resize(_swapchain_image_count);

	ErrorCheck(vkGetSwapchainImagesKHR(device, _swapchain, &_swapchain_image_count, _swapchain_images.data()));

	for (uint32_t i = 0; i < _swapchain_image_count; ++i) {
		VkImageViewCreateInfo image_view_create_info{};
		image_view_create_info.sType          = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.image          = _swapchain_images[i];
		image_view_create_info.viewType       = VK_IMAGE_VIEW_TYPE_2D;
		image_view_create_info.format         = _surface_format.format;
		image_view_create_info.components.r   = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.g   = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.b   = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.a   = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
		image_view_create_info.subresourceRange.baseMipLevel    = 0;
		image_view_create_info.subresourceRange.levelCount      = 1;
		image_view_create_info.subresourceRange.baseArrayLayer  = 0;
		image_view_create_info.subresourceRange.layerCount      = 1;

		ErrorCheck(vkCreateImageView(device, &image_view_create_info, nullptr, &_swapchain_images_views[i]));
	}
}

void Window::_DeInitSwapchainImages()
{
	auto device = _renderer->GetVulkanDevice();

	for (auto view : _swapchain_images_views) {
		vkDestroyImageView(device, view, nullptr);
	}
}

void Window::_InitDepthStencilImage()
{
	auto device = _renderer->GetVulkanDevice();
	
	{
		std::vector<VkFormat> try_formats{ 
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM
		};
		for (auto f : try_formats) {
			VkFormatProperties format_properties{};
			vkGetPhysicalDeviceFormatProperties(_renderer->GetVulkanPhysicalDevice(), f, &format_properties);
			if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				_depth_stencil_format = f;
				break;
			}
		}
		if (_depth_stencil_format == VK_FORMAT_UNDEFINED) {
			assert(0 && "Depth stencil format not selected.");
			std::exit(-1);
		}
		if ((_depth_stencil_format == VK_FORMAT_D32_SFLOAT_S8_UINT) ||
			(_depth_stencil_format == VK_FORMAT_D24_UNORM_S8_UINT) ||
			(_depth_stencil_format == VK_FORMAT_D16_UNORM_S8_UINT) ||
			(_depth_stencil_format == VK_FORMAT_S8_UINT)) {
			_stencil_avalible = true;
		}

	}

	VkImageCreateInfo image_create_info{};
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.flags = 0;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = _depth_stencil_format;
	image_create_info.extent.width = _surface_size_x;
	image_create_info.extent.height = _surface_size_y;
	image_create_info.extent.depth = 1;
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED;
	image_create_info.pQueueFamilyIndices = nullptr;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	vkCreateImage(device, &image_create_info, nullptr, &_depth_stencil_image);

	VkMemoryRequirements image_memory_requirement{};
	vkGetImageMemoryRequirements(device, _depth_stencil_image, &image_memory_requirement);

	uint32_t memory_index        = FindMemoryTypeIndex(
		&_renderer->GetVulkanPhysicalDeviceMemoryProperties(),
		&image_memory_requirement, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkMemoryAllocateInfo memory_allocate_info{};
	memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize = image_memory_requirement.size;
	memory_allocate_info.memoryTypeIndex = memory_index;

	vkAllocateMemory(device, &memory_allocate_info, nullptr, &_depth_stencil_image_memory);
	vkBindImageMemory(device, _depth_stencil_image, _depth_stencil_image_memory, 0);

	VkImageViewCreateInfo image_view_create_info{};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.image = _depth_stencil_image;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = _depth_stencil_format;
	image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
	(_stencil_avalible ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount = 1;

	vkCreateImageView(device, &image_view_create_info, nullptr, &_depth_stencil_image_view);
}

void Window::_DeInitDepthStencilImage()
{
	vkDestroyImageView(_renderer->GetVulkanDevice(), _depth_stencil_image_view, nullptr);
	vkFreeMemory(_renderer->GetVulkanDevice(), _depth_stencil_image_memory, nullptr);
	vkDestroyImage(_renderer->GetVulkanDevice(), _depth_stencil_image, nullptr);
}


void Window::_InitRenderPass()
{
	auto device = _renderer->GetVulkanDevice();

	std::array< VkAttachmentDescription, 2> attachments{};
	attachments[0].flags = 0;
	attachments[0].format = _depth_stencil_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	attachments[1].flags = 0;
	attachments[1].format = _surface_format.format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference sub_pass_0_depth_stancil_attachment{};
	sub_pass_0_depth_stancil_attachment.attachment = 0;
	sub_pass_0_depth_stancil_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array< VkAttachmentReference, 1> sub_pass_0_color_attachment{};
	sub_pass_0_color_attachment[0].attachment = 1;
	sub_pass_0_color_attachment[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array< VkSubpassDescription, 1> sub_passes{};
	sub_passes[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub_passes[0].colorAttachmentCount = sub_pass_0_color_attachment.size();
	sub_passes[0].pColorAttachments = sub_pass_0_color_attachment.data();
	sub_passes[0].pDepthStencilAttachment = &sub_pass_0_depth_stancil_attachment;

	VkRenderPassCreateInfo render_pass_create_info{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = attachments.size();
	render_pass_create_info.pAttachments = attachments.data();
	render_pass_create_info.subpassCount = sub_passes.size();
	render_pass_create_info.pSubpasses = sub_passes.data();

	ErrorCheck(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &_render_pass));

}

void Window::_DeInitRednderPass()
{
	vkDestroyRenderPass(_renderer->GetVulkanDevice(), _render_pass, nullptr);
}

void Window::_InitFramebuffers()
{
	_framebuffer.resize(_swapchain_image_count);
	for(uint32_t i = 0; i < _swapchain_image_count; ++i){
		std::array<VkImageView, 2> attachments{};
		attachments[0] =  _depth_stencil_image_view;
		attachments[1] = _swapchain_images_views[i];

		VkFramebufferCreateInfo _framebuffer_create_info{};
		_framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		_framebuffer_create_info.renderPass = _render_pass;
		_framebuffer_create_info.attachmentCount = attachments.size();
		_framebuffer_create_info.pAttachments = attachments.data();
		_framebuffer_create_info.width =  _surface_size_x;
		_framebuffer_create_info.height = _surface_size_y;
		_framebuffer_create_info.layers = 1;

		ErrorCheck(vkCreateFramebuffer(_renderer->GetVulkanDevice(), &_framebuffer_create_info, nullptr, &_framebuffer[i]));
	}
}

void Window::_DeInitFramebuffers()
{
	for (auto f : _framebuffer) {
		vkDestroyFramebuffer(_renderer->GetVulkanDevice(), f, nullptr);
	}
}

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	
	return buffer;
}

void Window::_CreateGraphicsPipeline()
{
	auto device = _renderer->GetVulkanDevice();
	auto vertShaderCode = readFile("../shaders/vert.spv");
	auto fragShaderCode = readFile("../shaders/frag.spv");

	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)_surface_size_x;
	viewport.height = (float)_surface_size_y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = GetVulkanSurfaceSize();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkDynamicState dynamicStates[] = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create pipeline layout!");
	}
	else {
		std::cout << "Vulkan: Pipeline layout created seccessfully" << std::endl;
	}

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencelStateCreateInfo{};
	pipelineDepthStencelStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencelStateCreateInfo.depthTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &pipelineDepthStencelStateCreateInfo; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = _render_pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create graphics pipeline!");
	}
	else {
		std::cout << "Vulkan: Graphics pipelines created seccessfully" << std::endl;
	}

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	std::cout << "Vulkan: Frag shader module destroyed seccessfully" << std::endl;
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	std::cout << "Vulkan: Vert shader module destroyed seccessfully" << std::endl;
}

void Window::_DestroyGraphicsPipeline()
{
	auto device = _renderer->GetVulkanDevice();
	vkDestroyPipeline(device, _graphicsPipeline, nullptr);
	std::cout << "Vulkan: Graphics pipelines destroyed seccessfully" << std::endl;
	vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
	std::cout << "Vulkan: Pipelines layout destroyed seccessfully" << std::endl;
}

void Window::_CreateCommandPool()
{
	auto device = _renderer->GetVulkanDevice();

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = _renderer->GetVulkanGraphicsQueueFamilyIndex();
	poolInfo.flags = 0; // Optional

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create command pool!");
	}
	else {
		std::cout << "Vulkan: Command pool created seccessfully" << std::endl;
	}

}

void Window::_DestroyCommandPool()
{
	auto device = _renderer->GetVulkanDevice();
	vkDestroyCommandPool(device, _commandPool, nullptr);
	std::cout << "Vulkan: Command pool destroyed seccessfully" << std::endl;
}

void Window::_CreateCommandBuffers()
{
	auto device = _renderer->GetVulkanDevice();
	_commandBuffers.resize(_framebuffer.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to allocate command buffers!");
	}
	else {
		std::cout << "Vulkan: Command buffers allocate seccessfully" << std::endl;
	}

	for (size_t i = 0; i < _commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Vulkan: Failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _render_pass;
		renderPassInfo.framebuffer = _framebuffer[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = GetVulkanSurfaceSize();

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		vkCmdBindPipeline(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(_commandBuffers[i], 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(_commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT16);

		vkCmdBindDescriptorSets(_commandBuffers[i], 
			VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

		vkCmdDrawIndexed(_commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(_commandBuffers[i]);

		if (vkEndCommandBuffer(_commandBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Vulkan: Failed to record command buffer!");
		}
	}
}

void Window::_DestroyCommandBuffers()
{
	vkFreeCommandBuffers(_renderer->GetVulkanDevice(), _commandPool, static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());
	std::cout << "Vulkan: Comman pool was free seccessfully" << std::endl;
}

void Window::createSyncObjects()
{
	auto device = _renderer->GetVulkanDevice();
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(_swapchain_images.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

			throw std::runtime_error("Vulkan: Failed to create synchronization objects for a frame!");
		}
	}
	std::cout << "Vulkan: Synchronization objects created seccessfully" << std::endl;
}

void Window::destroySyncObjects()
{
	auto device = _renderer->GetVulkanDevice();
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, inFlightFences[i], nullptr);
		std::cout << "Vulkan: Destroyed semaphore and fance seccessfully" << std::endl;
	}
}

void Window::cleanup()
{
	auto device = _renderer->GetVulkanDevice();
	cleanupSwapChain();

	destroyDescriptorSetLayout();

	destroyIndexBuffer();
	destroyVertexBuffer();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, inFlightFences[i], nullptr);
		std::cout << "Vulkan: Destroyed semaphore and fance seccessfully" << std::endl;
	}

	_DestroyCommandPool();

	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(_renderer->GetVulkanInstance(), _surface, nullptr);
	vkDestroyInstance(_renderer->GetVulkanInstance(), nullptr);

	_DeInitOSWindow();
}

void Window::recreateSwapChain()
{
	vkQueueWaitIdle(_renderer->GetVulkanQueue());

	_InitSwapchain();
	_InitSwapchainImages();
	_InitRenderPass();
	_CreateGraphicsPipeline();
	_InitFramebuffers();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	_CreateCommandBuffers();
}

void Window::cleanupSwapChain()
{
	_DeInitFramebuffers();
	_DestroyCommandBuffers();
	_DestroyGraphicsPipeline();
	_DeInitRednderPass();
	_DeInitSwapchainImages();
	_DeinitSwapchain();
	destroyUniformBuffers();
	destroyDescriptorPool();
}

void Window::createVertexBuffer()
{
	auto device = _renderer->GetVulkanDevice();
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT 
		| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	std::cout << "Vulkan: Create vertex buffer seccessfully" << std::endl;
}

void Window::destroyVertexBuffer()
{
	vkDestroyBuffer(_renderer->GetVulkanDevice(), vertexBuffer, nullptr);
	vkFreeMemory(_renderer->GetVulkanDevice(), vertexBufferMemory, nullptr);
	std::cout << "Vulkan: Destroyed vertex buffer seccessfully" << std::endl;
}

void Window::createIndexBuffer()
{
	auto device = _renderer->GetVulkanDevice();
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	std::cout << "Vulkan: Create index buffer seccessfully" << std::endl;
}

void Window::destroyIndexBuffer()
{
	auto device = _renderer->GetVulkanDevice();
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	std::cout << "Vulkan: Destroyed index buffer seccessfully" << std::endl;
}

void Window::createDescriptorSetLayout()
{
	auto device = _renderer->GetVulkanDevice();
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create descriptor set layout!");
	}
}

void Window::destroyDescriptorSetLayout()
{
	vkDestroyDescriptorSetLayout(_renderer->GetVulkanDevice(), descriptorSetLayout, nullptr);
	std::cout << "Vulkan: Destroyed description set layout seccessfully" << std::endl;
}

void Window::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniformBuffers.resize(_swapchain_images.size());
	uniformBuffersMemory.resize(_swapchain_images.size());

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		std::cout << "Vulkan: Create uniform buffer seccessfully" << std::endl;
	}
}

void Window::destroyUniformBuffers()
{
	auto device = _renderer->GetVulkanDevice();
	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		std::cout << "Vulkan: Destroy uniform buffer seccessfully" << std::endl;
	}
}

void Window::createDescriptorPool()
{
	auto device = _renderer->GetVulkanDevice();

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(_swapchain_images.size());

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	poolInfo.maxSets = static_cast<uint32_t>(_swapchain_images.size());
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create descriptor pool!");
	}

}

void Window::destroyDescriptorPool()
{
	auto device = _renderer->GetVulkanDevice();
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	std::cout << "Vulkan: Destroy description poll seccessfully" << std::endl;

}

void Window::createDescriptorSets()
{
	auto device = _renderer->GetVulkanDevice();
	std::vector<VkDescriptorSetLayout> layouts(_swapchain_images.size(), descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(_swapchain_images.size());
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(_swapchain_images.size());
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = nullptr; // Optional
		descriptorWrite.pTexelBufferView = nullptr; // Optional

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}
}

void Window::updateUniformBuffer(uint32_t currentImage)
{
	auto device = _renderer->GetVulkanDevice();

	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, 
		std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), 
		time * glm::radians(150.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), 
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	ubo.proj = glm::perspective(glm::radians(45.0f), 
		_surface_size_x / (float)_surface_size_y, 0.1f, 10.0f);

	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
}

uint32_t Window::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(_renderer->GetVulkanPhysicalDevice(), &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Vulkan: Failed to find suitable memory type!");
}

void Window::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	auto device = _renderer->GetVulkanDevice();
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Window::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	auto device = _renderer->GetVulkanDevice();
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = _commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(_renderer->GetVulkanQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(_renderer->GetVulkanQueue());
	vkFreeCommandBuffers(device, _commandPool, 1, &commandBuffer);


}

VkShaderModule Window::CreateShaderModule(const std::vector<char>& code)
{
	auto device = _renderer->GetVulkanDevice();
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	
	if (vkCreateShaderModule(device, &createInfo, nullptr, &_shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("Vulkan: Failed to create shader module!");
	}
	else{
		std::cout << "Vulkan: Shader module created seccessfully" << std::endl;
	} 
	return _shaderModule;
}



