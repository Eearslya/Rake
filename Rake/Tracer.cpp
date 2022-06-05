#include "Tracer.hpp"

#include <Luna/Utility/Log.hpp>
#include <Luna/Utility/Time.hpp>
#include <Tracy.hpp>

#include "IMaterial.hpp"
#include "ISkyMaterial.hpp"
#include "Random.hpp"
#include "Ray.hpp"
#include "World.hpp"

using Luna::Log;

static inline uint64_t ConstructTask(uint16_t yMin, uint16_t yMax, uint32_t sample) {
	return static_cast<uint64_t>(static_cast<uint64_t>(sample) | (static_cast<uint64_t>(yMax) << 32) |
	                             (static_cast<uint64_t>(yMin) << 48));
}

static inline void DeconstructTask(uint64_t task, uint16_t& yMin, uint16_t& yMax, uint32_t& sample) {
	yMin   = static_cast<uint16_t>((task >> 48) & 0xffff);
	yMax   = static_cast<uint16_t>((task >> 32) & 0xffff);
	sample = static_cast<uint32_t>(task & 0xffffffff);
}

Tracer::Tracer() {
	const auto threadCount = std::max(std::thread::hardware_concurrency() - 2u, 1u);
	Log::Info("Tracer", "Starting {} render threads.", threadCount);
	_running = true;
	for (int i = 0; i < threadCount; ++i) {
		_renderThreads.emplace_back([this, i]() { RenderThread(i + 1); });
	}
}

Tracer::~Tracer() noexcept {
	{
		std::unique_lock<std::mutex> lock(_tasksMutex);
		_running = false;
	}
	_tasksCondition.notify_all();
	for (auto& thread : _renderThreads) { thread.join(); }
}

bool Tracer::StartTrace(const glm::uvec2& imageSize, uint32_t samplesPerPixel, const std::shared_ptr<World>& world) {
	if (_rendering) { return false; }

	constexpr uint32_t linesPerTask = 10;

	Log::Info("Tracer", "Starting raytrace task.");
	Log::Info("Tracer", "- Image Size: {} x {}", imageSize.x, imageSize.y);
	Log::Info("Tracer", "- Samples Per Pixel: {}", samplesPerPixel);
	Log::Info("Tracer", "- World: {}", world->Name);
	Log::Info("Tracer", "- Lines Per Task: {}", linesPerTask);

	// Set our initial parameters.
	const double aspectRatio = static_cast<double>(imageSize.x) / static_cast<double>(imageSize.y);
	_camera                  = Camera(world->CameraPos,
                   world->CameraTarget,
                   world->VerticalFOV,
                   aspectRatio,
                   world->CameraAperture,
                   world->CameraFocusDistance);
	_totalRaycasts           = 0;
	_imageSize               = imageSize;
	_rendering               = true;
	_samplesPerPixel         = samplesPerPixel;
	_world                   = world;

	// Initialize our canvas.
	_pixels.resize(_imageSize.x * _imageSize.y);
	std::fill(_pixels.begin(), _pixels.end(), Color(0));
	_avgPixels.resize(_imageSize.x * _imageSize.y);
	std::fill(_avgPixels.begin(), _avgPixels.end(), Color(0));

	// Construct our world BVH.
	{
		Luna::Utility::ElapsedTime bvhTime;
		bvhTime.Update();
		_world->ConstructBVH();
		bvhTime.Update();
		Log::Info("Tracer", "Constructed world BVH in {}ms.", bvhTime.Get().AsMilliseconds<float>());
	}

	// Dispatch our first round of render tasks.
	_taskGroupCount    = 0;
	_lastUpdatedSample = 0;
	_completedSamples  = 0;
	uint32_t remaining = _imageSize.y;
	{
		std::lock_guard<std::mutex> lock(_tasksMutex);
		while (remaining > 0) {
			const uint32_t yMin = _imageSize.y - remaining;
			const uint32_t yMax = std::min(yMin + linesPerTask, _imageSize.y);
			remaining           = _imageSize.y - yMax;

			const auto task = ConstructTask(yMin, yMax, 0);
			_tasks.push(task);
			++_taskGroupCount;
		}
		_neededSamples = _taskGroupCount * _samplesPerPixel;
		_renderTime.Start();
		_tasksCondition.notify_all();
	}

	return true;
}

bool Tracer::CancelTrace() {
	if (!_rendering) { return true; }

	Log::Info("Tracer", "Cancelling raytrace task.");
	{
		std::lock_guard<std::mutex> lock(_tasksMutex);
		while (!_tasks.empty()) { _tasks.pop(); }
		_rendering = false;
	}

	return true;
}

void Tracer::Update() {
	if (_rendering) {
		_renderTime.Update();
		if (_completedSamples == _neededSamples) {
			_renderTime.Stop();
			_rendering = false;
			_world.reset();
			Log::Info("Tracer", "Raytrace task completed in {}ms.", _renderTime.Get().AsMilliseconds<float>());
		}
	}
}

bool Tracer::UpdatePixels(std::vector<Color>& pixels) {
	bool update = (_lastUpdatedSample + 100) < _completedSamples;
	update |= _completedSamples == _neededSamples && _lastUpdatedSample != _completedSamples;

	if (update) {
		_lastUpdatedSample = _completedSamples;
		pixels             = _avgPixels;
	}

	return update;
}

void Tracer::RenderThread(int threadID) {
	while (_running) {
		uint64_t task = 0;
		{
			std::unique_lock<std::mutex> lock(_tasksMutex);
			_tasksCondition.wait(lock, [this]() { return !_running || !_tasks.empty(); });

			if (!_running && _tasks.empty()) { break; }

			task = _tasks.front();
			_tasks.pop();
		}

		uint16_t yMin;
		uint16_t yMax;
		uint32_t sample;
		DeconstructTask(task, yMin, yMax, sample);
		const float avgFactor = 1.0f / (static_cast<float>(sample) + 1.0f);
		uint64_t raycasts     = 0;

		{
			const uint32_t width  = _imageSize.x;
			const uint32_t height = _imageSize.y;
			for (uint32_t y = yMin; y < yMax; ++y) {
				for (uint32_t x = 0; x < width; ++x) {
					const Color rayColor = Sample(glm::uvec2(x, y), _imageSize, _camera, *_world, raycasts);

					const auto offset = (y * width) + x;
					_pixels[offset] += rayColor;
					_avgPixels[offset] = _pixels[offset] * avgFactor;
				}
			}
		}

		_completedSamples.fetch_add(1, std::memory_order_relaxed);
		_totalRaycasts.fetch_add(raycasts, std::memory_order_acq_rel);
		if (_rendering && ++sample < _samplesPerPixel) {
			std::unique_lock<std::mutex> lock(_tasksMutex);
			const auto task = ConstructTask(yMin, yMax, sample);
			_tasks.push(task);
			_tasksCondition.notify_one();
		}
	}
}

Color Tracer::Sample(
	const glm::uvec2& coords, const glm::uvec2& imageSize, const Camera& camera, const World& world, uint64_t& raycasts) {
	const auto s  = (double(coords.x) + RandomDouble()) / (imageSize.x - 1);
	const auto t  = 1.0 - ((double(coords.y) + RandomDouble()) / (imageSize.y - 1));
	const Ray ray = camera.GetRay(s, t);

	return CastRay(ray, world, raycasts, 0);
}

Color Tracer::CastRay(const Ray& ray, const World& world, uint64_t& raycasts, uint32_t depth) {
	constexpr static uint32_t MaxDepth = 50;

	if (depth >= MaxDepth) { return Color(0.0); }
	++raycasts;

	HitRecord hit;
	if (world.BVH->Hit(ray, 0.001, Infinity, hit)) {
		Color attenuation;
		Ray scattered;
		const Color emission = hit.Material->Emit(hit.UV, hit.Point);
		if (hit.Material->Scatter(ray, hit, attenuation, scattered)) {
			return emission + attenuation * CastRay(scattered, world, raycasts, depth + 1);
		} else {
			return emission;
		}
	} else {
		return world.Sky->Sample(ray);
	}
}
