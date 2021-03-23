#pragma once

#include "Platform.h"

#include <iostream>
#include<assert.h>

void ErrorCheck(VkResult result);

uint32_t FindMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties *gpu_memory_properties, const VkMemoryRequirements *memory_requirements,const VkMemoryPropertyFlags memory_properties);