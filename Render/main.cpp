#pragma once
#include"Renderer.h"
#include"Window.h"
#include"GltfLoader.h"

int main()
{
	Renderer r;

	GltfLoader * gltf = new GltfLoader(&r);

	auto w = r.OpenWindow(800, 600, "test", gltf);

	float color_rotator = 0.0f;
	auto timer = std::chrono::steady_clock();
	auto last_time = timer.now();
	uint64_t frame_counter = 0;
	uint64_t fps = 0;

	while (r.Run()) {

		++frame_counter;
		if (last_time + std::chrono::seconds(1) < timer.now()) {
			last_time = timer.now();
			fps = frame_counter;
			frame_counter = 0;
			std::cout << "FPS:" << fps << std::endl;
		}

		w->DrawFrame();
	}
	return 0;
}