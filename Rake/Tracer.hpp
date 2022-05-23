#pragma once

#include <rigtorp/SPSCQueue.h>

#include <memory>
#include <thread>

#include "RenderMessages.hpp"

struct World;

struct RenderContext {
	RenderContext() = default;
	RenderContext(const RenderRequest& request);

	bool IsComplete() const;

	unsigned int CurrentSample;
	uint64_t Raycasts;
	RenderRequest Request;
	unsigned int SampleCount;
	std::shared_ptr<World> World;
};

struct SubRenderRequest {
	SubRenderRequest(RenderContext& context) : Context(context) {}

	RenderContext& Context;
	uint32_t MinY = 0;
	uint32_t MaxY = 0;
};

struct SubRenderComplete {
	uint64_t Raycasts;
};

class RenderThread {
 public:
	RenderThread();
	~RenderThread() noexcept;

	bool IsRunning() const {
		return _rendering;
	}
	void SetThreadID(uint32_t threadID) {
		_threadID = threadID;
	}

	rigtorp::SPSCQueue<RenderCancel> Cancels;
	rigtorp::SPSCQueue<SubRenderRequest> Requests;
	rigtorp::SPSCQueue<RenderResult> Results;

 private:
	void RenderWork();

	std::atomic_bool _rendering = false;
	std::atomic_bool _shutdown  = false;
	std::thread _thread;
	uint32_t _threadID;
};

class Tracer {
 public:
	Tracer();
	~Tracer() noexcept;

	bool IsRunning() const {
		return _rendering;
	}

	rigtorp::SPSCQueue<RenderCancel> Cancels;
	rigtorp::SPSCQueue<RenderComplete> Completes;
	rigtorp::SPSCQueue<RenderRequest> Requests;
	rigtorp::SPSCQueue<RenderResult> Results;
	rigtorp::SPSCQueue<RenderStatus> Status;

 private:
	void DispatchThread();

	RenderContext _activeContext;
	std::atomic_bool _rendering = false;
	std::atomic_bool _shutdown  = false;
	std::thread _dispatchThread;
	std::vector<RenderThread> _renderThreads;
	std::vector<unsigned int> _samplesDone;
	unsigned int _workingRenderThreads = 0;
	uint64_t _totalRaycasts            = 0;
};
