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
	uint32_t MinY;
	uint32_t MaxY;
	uint32_t Width;
	unsigned int SampleCount;
	std::vector<Color> Pixels;
	uint64_t Raycasts;
	bool Complete;
};

struct RenderStatus {
	unsigned int FinishedSamples;
	uint64_t TotalRaycasts;
};

struct RenderComplete {};
