#pragma once

#include <Luna/Utility/Time.hpp>
#include <atomic>
#include <condition_variable>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Camera.hpp"
#include "DataTypes.hpp"

class World;

class Tracer {
 public:
	Tracer();
	~Tracer() noexcept;

	Luna::Utility::Time GetElapsedTime() const {
		return _renderTime.Get();
	}
	bool IsRunning() const {
		return _rendering;
	}

	bool StartTrace(const glm::uvec2& imageSize, uint32_t samplesPerPixel, const std::shared_ptr<World>& world);
	bool CancelTrace();
	void Update();
	bool UpdatePixels(std::vector<Color>& pixels);

 private:
	void RenderThread(int threadID);

	static Color CastRay(const Ray& ray, const World& world, uint32_t depth);

	glm::uvec2 _imageSize = glm::uvec2(0);
	std::vector<Color> _pixels;
	std::vector<Color> _avgPixels;
	std::atomic_bool _rendering = false;
	std::atomic_bool _running   = false;
	std::vector<std::thread> _renderThreads;
	uint32_t _samplesPerPixel = 0;
	Camera _camera;
	std::shared_ptr<World> _world;

	std::atomic_uint64_t _completedSamples;
	uint32_t _taskGroupCount    = 0;
	uint64_t _neededSamples     = 0;
	uint64_t _lastUpdatedSample = 0;
	std::queue<uint64_t> _tasks;
	std::mutex _tasksMutex;
	std::condition_variable _tasksCondition;

	Luna::Utility::Stopwatch _renderTime;
};
