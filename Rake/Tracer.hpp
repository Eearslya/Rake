#pragma once

#include <rigtorp/SPSCQueue.h>

#include <memory>
#include <thread>

#include "Camera.hpp"
#include "DataTypes.hpp"
#include "Ray.hpp"
#include "RenderMessages.hpp"

struct World;

struct RenderContext {
	RenderContext(const RenderRequest& request);

	bool IsComplete() const;

	Camera Cam;
	unsigned int CurrentSample;
	std::vector<Color> Pixels;
	uint64_t Raycasts;
	RenderRequest Request;
	unsigned int SampleCount;
	std::shared_ptr<World> World;
};

class Tracer {
 public:
	Tracer();
	~Tracer() noexcept;

	bool IsRunning() const {
		return bool(_activeContext);
	}

	rigtorp::SPSCQueue<RenderCancel> Cancels;
	rigtorp::SPSCQueue<RenderRequest> Requests;
	rigtorp::SPSCQueue<RenderResult> Results;

 private:
	constexpr static uint32_t MaxDepth = 50;

	void WorkerThread();

	bool CancelRequested();
	void SendRenderResult();
	void TryGetRequest();
	void WorkRequest();

	Color CastRay(const Ray& ray, const World& world, uint64_t& raycasts, uint32_t depth);

	std::unique_ptr<RenderContext> _activeContext;
	std::atomic_bool _shutdown = false;
	std::thread _workerThread;
};
