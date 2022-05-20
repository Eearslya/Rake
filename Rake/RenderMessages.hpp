#pragma once

#include <vector>

#include "DataTypes.hpp"

struct RenderCancel {};

struct RenderRequest {
	glm::uvec2 ImageSize;
	unsigned int SampleCount;
};

struct RenderResult {
	glm::uvec2 ImageSize;
	unsigned int SampleCount;
	std::vector<Color> Pixels;
	uint64_t Raycasts;
	bool RenderComplete;
};
