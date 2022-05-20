#include <Luna/Graphics/Graphics.hpp>
#include <Luna/Graphics/Vulkan/CommandBuffer.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/RenderPass.hpp>
#include <Luna/Graphics/Vulkan/Swapchain.hpp>
#include <Luna/Utility/Log.hpp>

namespace Luna {
Graphics::Graphics() {
	_device    = std::make_unique<Vulkan::Device>();
	_swapchain = std::make_unique<Vulkan::Swapchain>(*_device);
}

Graphics::~Graphics() noexcept {}

void Graphics::Update() {
	if (!BeginFrame()) { return; }

	OnRender();

	EndFrame();
}

bool Graphics::BeginFrame() {
	auto window = Window::Get();
	if (window->IsIconified()) { return false; }

	_device->NextFrame();

	const auto acquired = _swapchain->AcquireNextImage();

	return acquired;
}

void Graphics::EndFrame() {
	_device->EndFrame();
	_swapchain->Present();
}
}  // namespace Luna
