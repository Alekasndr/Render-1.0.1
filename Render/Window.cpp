#include "Window.h"
#include"Renderer.h"
#include"Shared.h"
#include<assert.h>
#include<array>
#include<fstream>

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
	_CreateGraphicsPipeline();
	_InitFramebuffers();
	_InitSynchronization();
	
}

Window::~Window()
{
	vkQueueWaitIdle(_renderer->GetVulkanQueue());
	
	_DeInitSynchronization();
	_DeInitFramebuffers();
	_DestroyGraphicsPipeline();
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

void Window::BeginRender()
{
	auto device = _renderer->GetVulkanDevice();
	ErrorCheck(vkAcquireNextImageKHR(device, _swapchain, 
		UINT64_MAX, VK_NULL_HANDLE, 
		_swapchain_image_available, 
		&_active_swapchain_image_id));
	ErrorCheck(vkWaitForFences(device, 1, &_swapchain_image_available, VK_TRUE, UINT64_MAX));
	ErrorCheck(vkResetFences(device, 1, &_swapchain_image_available));
	ErrorCheck(vkQueueWaitIdle(_renderer->GetVulkanQueue()));

}

void Window::EndRender(std::vector<VkSemaphore> wait_semaphore)
{
	VkResult present_result = VkResult::VK_RESULT_MAX_ENUM;

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = wait_semaphore.size();
	present_info.pWaitSemaphores = wait_semaphore.data();
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &_swapchain;
	present_info.pImageIndices = &_active_swapchain_image_id;
	present_info.pResults = &present_result;



	ErrorCheck(vkQueuePresentKHR(_renderer->GetVulkanQueue(), &present_info));
	ErrorCheck(present_result);
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

void Window::_InitSynchronization()
{
	VkFenceCreateInfo fence_create_info{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	vkCreateFence(_renderer->GetVulkanDevice(), &fence_create_info, nullptr, &_swapchain_image_available);
	
}

void Window::_DeInitSynchronization()
{
	vkDestroyFence(_renderer->GetVulkanDevice(), _swapchain_image_available, nullptr);
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
	auto vertShaderCode = readFile("D:/Render 1.0.1/shaders/vert.spv");
	auto fragShaderCode = readFile("D:/Render 1.0.1/shaders/frag.spv");

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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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

	std::cout << " viewport created seccessfully" << std::endl;

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
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}
	else {
		std::cout << "pipeline layout created seccessfully" << std::endl;
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
		throw std::runtime_error("failed to create graphics pipeline!");
	}
	else {
		std::cout << "graphics pipelines created seccessfully" << std::endl;
	}

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	std::cout << "frag shader module destroyed seccessfully" << std::endl;
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	std::cout << "vert shader module destroyed seccessfully" << std::endl;

}

void Window::_DestroyGraphicsPipeline()
{
	auto device = _renderer->GetVulkanDevice();
	vkDestroyPipeline(device, _graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
}

VkShaderModule Window::CreateShaderModule(const std::vector<char>& code)
{
	auto device = _renderer->GetVulkanDevice();
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	
	if (vkCreateShaderModule(device, &createInfo, nullptr, &_shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}
	else{
		std::cout << "shader module created seccessfully" << std::endl;
	} 

	return _shaderModule;
}



