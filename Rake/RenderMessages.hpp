#pragma once

#include <memory>
#include <vector>

#include "DataTypes.hpp"

class World;

struct RenderCancel {};

struct RenderRequest {
	glm::uvec2 ImageSize;
	unsigned int SampleCount;
	std::shared_ptr<World> World;
};

struct RenderResult {
	glm::uvec2 ImageSize;
	unsigned int SampleCount;
	std::vector<Color> Pixels;
	uint64_t Raycasts;
	bool RenderComplete;
};
