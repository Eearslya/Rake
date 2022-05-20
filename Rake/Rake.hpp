#pragma once

#include <Luna/Core/App.hpp>
#include <glm/glm.hpp>

class Rake : public Luna::App {
 public:
	Rake();
	Rake(const Rake&) = delete;
	~Rake() noexcept  = default;

	Rake& operator=(const Rake&) = delete;

	virtual void Start() override;
	virtual void Stop() override;
	virtual void Update() override;

 private:
	void BeginDockspace() const;
	void Render();
	void RenderRakeUI();
	void RenderViewport();

	glm::uvec2 _viewportSize = glm::uvec2(800, 600);
};
