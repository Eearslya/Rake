#include "Tracer.hpp"

#include <Luna/Utility/Log.hpp>

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
			World(request.World) {
	Pixels.resize(request.ImageSize.x * request.ImageSize.y);
	std::fill(Pixels.begin(), Pixels.end(), Color(0.0));
}

bool RenderContext::IsComplete() const {
	return CurrentSample >= SampleCount;
}

Tracer::Tracer() : Cancels(1), Requests(1), Results(1) {
	Log::Info("Tracer", "Starting worker thread.");
	_workerThread = std::thread([this]() { WorkerThread(); });
}

Tracer::~Tracer() noexcept {
	_shutdown = true;
	Log::Info("Tracer", "Stopping worker thread.");
	_workerThread.join();
}

void Tracer::WorkerThread() {
	while (!_shutdown) {
		if (_activeContext) {
			WorkRequest();
		} else {
			TryGetRequest();
		}
	}
}

bool Tracer::CancelRequested() {
	const bool cancel = Cancels.front() != nullptr;
	if (cancel) { Cancels.pop(); }

	return cancel;
}

void Tracer::SendRenderResult() {
	const auto& request = _activeContext->Request;
	const bool complete = _activeContext->IsComplete();

	RenderResult result{.ImageSize      = request.ImageSize,
	                    .SampleCount    = _activeContext->CurrentSample,
	                    .Pixels         = _activeContext->Pixels,
	                    .Raycasts       = _activeContext->Raycasts,
	                    .RenderComplete = complete};
	if (complete) {
		Log::Info("Tracer-Worker", "Render request completed.");

		// If this is the final result, ensure it doesn't get discarded.
		Results.emplace(std::move(result));
	} else {
		const bool sent = Results.try_emplace(std::move(result));
	}
}

void Tracer::TryGetRequest() {
	// If we're not actively working on something, check if there is a pending request.
	if (Requests.front()) {
		_activeContext = std::make_unique<RenderContext>(*Requests.front());
		Requests.pop();

		const auto& request = _activeContext->Request;
		Log::Info("Tracer-Worker", "Received render request.");
		Log::Info("Tracer-Worker", "- Image Size: {} x {}", request.ImageSize.x, request.ImageSize.y);
		Log::Info("Tracer-Worker", "- Samples Per Pixel: {}", request.SampleCount);
	}
}

void Tracer::WorkRequest() {
	if (CancelRequested()) {
		Log::Info("Tracer-Worker", "Received cancel request.");
		_activeContext.reset();
		return;
	}

	auto& pixels        = _activeContext->Pixels;
	auto& raycasts      = _activeContext->Raycasts;
	const auto& camera  = _activeContext->Cam;
	const auto& request = _activeContext->Request;
	const auto& world   = *request.World;
	const auto width    = request.ImageSize.x;
	const auto height   = request.ImageSize.y;

	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			const auto s         = (double(x) + RandomDouble()) / (width - 1);
			const auto t         = 1.0 - ((double(y) + RandomDouble()) / (height - 1));
			const Ray ray        = camera.GetRay(s, t);
			const Color rayColor = CastRay(ray, world, raycasts, 0);

			pixels[(y * request.ImageSize.x) + x] += rayColor;
		}
	}

	++_activeContext->CurrentSample;

	SendRenderResult();

	if (_activeContext->CurrentSample >= _activeContext->SampleCount) { _activeContext.reset(); }
}

Color Tracer::CastRay(const Ray& ray, const World& world, uint64_t& raycasts, uint32_t depth) {
	if (depth >= MaxDepth) { return Color(0.0); }
	++raycasts;

	HitRecord hit;
	if (world.Objects.Hit(ray, 0.001, Infinity, hit)) {
		const Point3 target = hit.Point + RandomInHemisphere(hit.Normal);
		return 0.5f * CastRay(Ray(hit.Point, glm::normalize(target - hit.Point)), world, raycasts, depth + 1);
	} else {
		return world.Sky;
	}
}
