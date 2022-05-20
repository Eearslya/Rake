#pragma once

#include <Luna/Platform/Window.hpp>
#include <Luna/Utility/Delegate.hpp>
#include <Luna/Utility/Module.hpp>

namespace Luna {
namespace Vulkan {
class Device;
class Program;
class Swapchain;
}  // namespace Vulkan

class Graphics : public Utility::Module::Registrar<Graphics> {
	static inline const bool Registered = Register("Graphics", Stage::Render, Depends<Window>());

 public:
	Graphics();
	~Graphics() noexcept;

	Vulkan::Device& GetDevice() const {
		return *_device;
	}
	Vulkan::Swapchain& GetSwapchain() const {
		return *_swapchain;
	}

	void Update() override;

	Utility::Delegate<void()> OnRender;

 private:
	bool BeginFrame();
	void EndFrame();

	std::unique_ptr<Vulkan::Device> _device;
	std::unique_ptr<Vulkan::Swapchain> _swapchain;
};
}  // namespace Luna
