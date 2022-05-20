#pragma once

#include <Luna/Core/App.hpp>
#include <Luna/Graphics/Vulkan/Common.hpp>
#include <Luna/Utility/Time.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Tracer;
class World;

class Rake : public Luna::App {
 public:
	Rake();
	Rake(const Rake&) = delete;
	~Rake() noexcept;

	Rake& operator=(const Rake&) = delete;

	virtual void Start() override;
	virtual void Stop() override;
	virtual void Update() override;

 private:
	void Render();
	void RequestCancel();
	void RequestTrace();

	void RenderRakeUI();
	void RenderDockspace();
	void RenderControls();
	void RenderViewport();

	Luna::Vulkan::ImageHandle _renderImage;
	Luna::Utility::Stopwatch _renderTime;
	std::unique_ptr<Tracer> _tracer;
	glm::uvec2 _viewportSize = glm::uvec2(800, 600);

	unsigned int _currentWorld = 0;
	std::vector<std::shared_ptr<World>> _worlds;

	unsigned int _samplesPerPixel = 100;

	uint64_t _raysCompleted        = 0;
	unsigned int _samplesCompleted = 0;
	unsigned int _samplesRequested = 0;
};
