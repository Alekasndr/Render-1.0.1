#include"Renderer.h"
#include"Window.h"
#include<array>
#include<chrono>
#include<iostream>
using namespace std;

constexpr double PI = 3.141592653589793238462643;
constexpr double CIRCLE_RED = PI * 2;
constexpr double CIRCLE_THIRD = CIRCLE_RED / 3.0;
constexpr double CIRCLE_THIRD_1 = 0;
constexpr double CIRCLE_THIRD_2 = CIRCLE_THIRD;
constexpr double CIRCLE_THIRD_3 = CIRCLE_THIRD * 2;

int main()
{
	Renderer r;
	
	auto w = r.OpenWindow(800, 600, "test");

	VkCommandPool comand_pool;
	VkCommandPoolCreateInfo pool_create_info{};
	pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_create_info.queueFamilyIndex = r.GetVulkanGraphicsQueueFamilyIndex();
	pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	vkCreateCommandPool(r.GetVulkanDevice(), &pool_create_info, nullptr, &comand_pool);

	VkCommandBuffer comand_buffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo comand_buffer_allocate_info{};
	comand_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	comand_buffer_allocate_info.commandPool = comand_pool;
	comand_buffer_allocate_info.commandBufferCount = 1;
	comand_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkAllocateCommandBuffers(r.GetVulkanDevice(), &comand_buffer_allocate_info, &comand_buffer);

	VkSemaphore render_complete_semaphore = VK_NULL_HANDLE;
	VkSemaphoreCreateInfo semaphore_create_info{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(r.GetVulkanDevice(), &semaphore_create_info, nullptr, &render_complete_semaphore);

	float color_rotator = 0.0f;
	auto timer = std::chrono::steady_clock();
	auto last_time = timer.now();
	uint64_t frame_counter = 0;
	uint64_t fps = 0;

	while (r.Run()) {
		//CPU logic calculation

		++frame_counter;
		if (last_time + std::chrono::seconds(1) < timer.now()) {
			last_time = timer.now();
			fps = frame_counter;
			frame_counter = 0;
			std::cout << "FPS:" << fps << std::endl;
		}

		//Began render
		w->BeginRender();
		//Record command buffer
		VkCommandBufferBeginInfo comand_buffer_begin_info{};
		comand_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		comand_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(comand_buffer, &comand_buffer_begin_info);
		
		VkRect2D render_area{};
		render_area.offset.x = 0;
		render_area.offset.y = 0;
		render_area.extent = w->GetVulkanSurfaceSize();

		color_rotator += 0.001;

		std::array<VkClearValue, 2> clear_value{};
		clear_value[0].depthStencil.depth = 0.0f;
		clear_value[0].depthStencil.stencil = 0;
		clear_value[1].color.float32[0] = std::sin(color_rotator + CIRCLE_THIRD_1) * 0.5 + 0.5;
		clear_value[1].color.float32[1] = std::sin(color_rotator + CIRCLE_THIRD_2) * 0.5 + 0.5;
		clear_value[1].color.float32[2] = std::sin(color_rotator + CIRCLE_THIRD_3) * 0.5 + 0.5;
		clear_value[1].color.float32[3] = 1.0f;

		VkRenderPassBeginInfo render_pass_begiin_info{};
		render_pass_begiin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begiin_info.renderPass = w->GetVulkanRenderPass();
		render_pass_begiin_info.framebuffer = w->GetVulkanFramebuffer();
		render_pass_begiin_info.renderArea = render_area;
		render_pass_begiin_info.clearValueCount = clear_value.size();
		render_pass_begiin_info.pClearValues = clear_value.data();

		vkCmdBeginRenderPass(comand_buffer, &render_pass_begiin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdEndRenderPass(comand_buffer);

		vkEndCommandBuffer(comand_buffer);
		//Submit command buffer
		
		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pWaitSemaphores = 0;
		submit_info.pWaitSemaphores = nullptr;
		submit_info.pWaitDstStageMask = nullptr;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &comand_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &render_complete_semaphore;

			vkQueueSubmit(r.GetVulkanQueue(), 1, &submit_info, VK_NULL_HANDLE);
		
		//End render
		w->EndRender({render_complete_semaphore});
	}

	vkQueueWaitIdle(r.GetVulkanQueue());
	vkDestroySemaphore(r.GetVulkanDevice(), render_complete_semaphore, nullptr);
	vkDestroyCommandPool(r.GetVulkanDevice(), comand_pool, nullptr);

	return 0;
}