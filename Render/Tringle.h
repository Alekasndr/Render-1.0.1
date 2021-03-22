#pragma once

#include"Renderer.h"
#include"Window.h"
#include <fstream>


class Tringle
{
public:
	Tringle();
	~Tringle();


	Window* _window = nullptr;
	Renderer* _renderer = nullptr;
	void cleanup();
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;


private:

	static std::vector<char> readFile(const std::string& filename);


	void createGraphicsPipeline();


	VkShaderModule createShaderModule(const std::vector<char>& code);


};

