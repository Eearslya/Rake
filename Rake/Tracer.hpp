#pragma once

#include <rigtorp/SPSCQueue.h>

#include <memory>
#include <thread>

#include "DataTypes.hpp"
#include "RenderMessages.hpp"

struct RenderContext {
	RenderContext(const RenderRequest& request);

	bool IsComplete() const;

	unsigned int CurrentSample;
	std::vector<Color> Pixels;
	uint64_t Raycasts;
	RenderRequest Request;
	unsigned int SampleCount;
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
	void WorkerThread();

	bool CancelRequested();
	void SendRenderResult();
	void TryGetRequest();
	void WorkRequest();

	std::unique_ptr<RenderContext> _activeContext;
	std::atomic_bool _shutdown = false;
	std::thread _workerThread;
};
