#include "Tracer.hpp"

#include <Luna/Utility/Log.hpp>

#include "BVHNode.hpp"
#include "IMaterial.hpp"
#include "ISkyMaterial.hpp"
#include "Random.hpp"
#include "World.hpp"

using Luna::Log;

RenderContext::RenderContext(const RenderRequest& request)
		: Cam(request.World->CameraPos,
          request.World->CameraTarget,
          request.World->VerticalFOV,
          double(request.ImageSize.x) / double(request.ImageSize.y),
          request.World->CameraAperture,
          request.World->CameraFocusDistance),
			CurrentSample(0),
			Raycasts(0),
			Request(request),
			SampleCount(request.SampleCount),
			World(request.World) {}

bool RenderContext::IsComplete() const {
	return CurrentSample >= SampleCount;
}

RenderThread::RenderThread() : Cancels(1), Requests(1), Results(1) {
	_thread = std::thread([this]() { RenderWork(); });
}

RenderThread::~RenderThread() noexcept {
	_shutdown = true;
	_thread.join();
}

void RenderThread::RenderWork() {
	using namespace std::chrono_literals;

	while (!_shutdown) {
		if (Requests.front()) {
			SubRenderRequest request = *Requests.front();

			_rendering = true;

			const auto& context = request.Context;
			const auto& world   = *context.World;
			const auto width    = context.Request.ImageSize.x;
			const auto height   = context.Request.ImageSize.y;

			std::vector<Color> pixels(width * (request.MaxY - request.MinY) + 1);
			std::fill(pixels.begin(), pixels.end(), Color(0.0));

			uint64_t raycasts = 0;
			for (uint32_t s = 0; s < context.SampleCount; ++s) {
				for (uint32_t y = request.MinY; y < request.MaxY; ++y) {
					for (uint32_t x = 0; x < width; ++x) {
						const auto s         = (double(x) + RandomDouble()) / (width - 1);
						const auto t         = 1.0 - ((double(y) + RandomDouble()) / (height - 1));
						const Ray ray        = context.Cam.GetRay(s, t);
						const Color rayColor = CastRay(ray, world, raycasts, 0);

						pixels[((y - request.MinY) * width) + x] += rayColor;
					}
				}

				bool complete   = (s + 1) == context.SampleCount;
				bool sendResult = Results.empty();
				if (complete) { sendResult = true; }
				if (sendResult) {
					RenderResult result{.MinY        = request.MinY,
					                    .MaxY        = request.MaxY,
					                    .Width       = width,
					                    .SampleCount = s + 1,
					                    .Pixels      = pixels,
					                    .Raycasts    = raycasts,
					                    .ThreadID    = _threadID,
					                    .Complete    = complete};
					Results.push(std::move(result));
					raycasts = 0;
				}

				if (Cancels.front()) {
					Cancels.pop();
					break;
				}
				if (_shutdown) { break; }
			}

			Requests.pop();
			_rendering = false;
		} else {
			std::this_thread::sleep_for(25ms);
		}
	}
}

Color RenderThread::CastRay(const Ray& ray, const World& world, uint64_t& raycasts, uint32_t depth) {
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

Tracer::Tracer() : Cancels(1), Completes(1), Requests(1), Results(16), Status(1) {
	Log::Info("Tracer", "Starting dispatch thread.");
	_dispatchThread = std::thread([this]() { DispatchThread(); });

	const auto threadCount = std::thread::hardware_concurrency() - 2;
	Log::Info("Tracer", "Starting {} render threads.", threadCount);
	_renderThreads = std::vector<RenderThread>(threadCount);
	_samplesDone   = std::vector<unsigned int>(threadCount, 0);
	for (int t = 0; t < threadCount; ++t) { _renderThreads[t].SetThreadID(t); }
}

Tracer::~Tracer() noexcept {
	_shutdown = true;

	Log::Info("Tracer", "Stopping render threads.");
	_renderThreads.clear();

	Log::Info("Tracer", "Stopping dispatch thread.");
	_dispatchThread.join();
}

void Tracer::DispatchThread() {
	using namespace std::chrono_literals;

	while (!_shutdown) {
		if (_rendering) {
			if (Cancels.front()) {
				Cancels.pop();

				for (auto& thread : _renderThreads) {
					if (!thread.IsRunning()) { continue; }
					thread.Cancels.emplace();
					while (thread.Results.front()) { thread.Results.pop(); }
				}

				if (Requests.front()) { Requests.pop(); }

				_rendering = false;

				Log::Info("Tracer-Dispatch", "Received cancel request.");
			} else {
				bool updated = false;
				for (size_t threadID = 0; threadID < _renderThreads.size(); ++threadID) {
					auto& thread = _renderThreads[threadID];

					if (thread.Results.front()) {
						RenderResult result = *thread.Results.front();
						thread.Results.pop();
						updated                = true;
						_samplesDone[threadID] = result.SampleCount;
						_totalRaycasts += result.Raycasts;

						if (result.Complete) {
							// Always send completed results, even if the queue is full.
							Results.push(std::move(result));
							--_workingRenderThreads;
						} else {
							const bool sent = Results.try_push(std::move(result));
						}
					}
				}

				unsigned int samplesDone = _activeContext.SampleCount;
				for (const auto count : _samplesDone) { samplesDone = std::min(samplesDone, count); }

				const bool complete = _workingRenderThreads == 0;

				if (updated) {
					RenderStatus status{.FinishedSamples = samplesDone, .TotalRaycasts = _totalRaycasts};
					if (complete) {
						Status.push(std::move(status));
					} else {
						const bool sent = Status.try_push(std::move(status));
					}
				}

				if (complete) {
					Log::Info("Tracer-Dispatch", "Render request completed.");
					Requests.pop();
					_rendering = false;
					Completes.emplace();
				}

				std::this_thread::sleep_for(25ms);
			}
		} else {
			if (Requests.front()) {
				_activeContext = RenderContext(*Requests.front());
				_rendering     = true;
				_activeContext.World->ConstructBVH();

				const auto& request = _activeContext.Request;
				Log::Info("Tracer-Dispatch", "Received render request.");
				Log::Info("Tracer-Dispatch", "- Image Size: {} x {}", request.ImageSize.x, request.ImageSize.y);
				Log::Info("Tracer-Dispatch", "- Samples Per Pixel: {}", request.SampleCount);

				const auto width               = request.ImageSize.x;
				const auto height              = request.ImageSize.y;
				const auto threadCount         = _renderThreads.size();
				const uint32_t heightPerThread = ceil(float(height) / float(threadCount));

				SubRenderRequest subRequest(_activeContext);
				std::vector<SubRenderRequest> subRequests;
				while (subRequest.MaxY < height) {
					subRequest.MinY = subRequest.MaxY;
					subRequest.MaxY = std::min(subRequest.MinY + heightPerThread, height);
					subRequests.push_back(subRequest);
				}
				const auto renderThreadCount = subRequests.size();
				Log::Info("Tracer-Dispatch", "Dispatching {} render threads.", renderThreadCount);
				std::fill(_samplesDone.begin(), _samplesDone.end(), 0);
				_totalRaycasts        = 0;
				_workingRenderThreads = renderThreadCount;

				for (size_t thread = 0; thread < renderThreadCount; ++thread) {
					_renderThreads[thread].Requests.push(subRequests[thread]);
				}
			} else {
				std::this_thread::sleep_for(25ms);
			}
		}
	}
}
