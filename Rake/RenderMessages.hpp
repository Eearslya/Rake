#pragma once

#include <memory>

class World;

struct RenderCancel {};

struct RenderRequest {
	unsigned int SampleCount;
	std::shared_ptr<World> World;
};

struct RenderResult {
	uint32_t MinY;
	uint32_t MaxY;
	uint32_t Width;
	unsigned int SampleCount;
	uint64_t Raycasts;
	bool Complete;
};

struct RenderStatus {
	unsigned int FinishedSamples;
	uint64_t TotalRaycasts;
};

struct RenderComplete {};
