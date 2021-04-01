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


	while (r.Run()) {
		//CPU logic calculation

		//Began render
		w->DrawFrame();
		
	}

	vkQueueWaitIdle(r.GetVulkanQueue());

	return 0;
}