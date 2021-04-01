#include"Renderer.h"
#include"Window.h"
#include<array>
#include<chrono>
#include<iostream>
using namespace std;

int main()
{
	Renderer r;
	
	auto w = r.OpenWindow(800, 600, "test");

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
		//Began render
		w->DrawFrame();
		
	}

	

	return 0;
}