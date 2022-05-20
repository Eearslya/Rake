#include <Luna/Graphics/Vulkan/CommandPool.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Utility/Log.hpp>

namespace Luna {
namespace Vulkan {
CommandPool::CommandPool(Device& device, uint32_t familyIndex, bool resettable) : _device(device) {
	const auto& dev = _device.GetDevice();

	vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eTransient;
	if (resettable) { flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer; }

	const vk::CommandPoolCreateInfo poolCI(flags, familyIndex);
	_pool = dev.createCommandPool(poolCI);

	Log::Trace("Vulkan::CommandPool", "Created new Command Pool. (Family: {}, Resettable: {})", familyIndex, resettable);
}

CommandPool::~CommandPool() noexcept {
	const auto& dev = _device.GetDevice();
	if (_pool) { dev.destroyCommandPool(_pool); }
}

vk::CommandBuffer CommandPool::RequestCommandBuffer() {
	const auto& dev = _device.GetDevice();
	if (_bufferIndex == _commandBuffers.size()) {
		const vk::CommandBufferAllocateInfo bufferAI(_pool, vk::CommandBufferLevel::ePrimary, 1);
		auto buffers = dev.allocateCommandBuffers(bufferAI);
		_commandBuffers.push_back(buffers[0]);
	}

	return _commandBuffers[_bufferIndex++];
}

void CommandPool::Reset() {
	if (_bufferIndex == 0) { return; }

	const auto& dev = _device.GetDevice();
	_bufferIndex    = 0;
	dev.resetCommandPool(_pool);
}

void CommandPool::Trim() {
	const auto& dev        = _device.GetDevice();
	const auto& extensions = _device.GetExtensionInfo();

	dev.resetCommandPool(_pool);
	if (!_commandBuffers.empty()) { dev.freeCommandBuffers(_pool, _commandBuffers); }
	_bufferIndex = 0;
	_commandBuffers.clear();
	if (extensions.Maintenance1) { dev.trimCommandPoolKHR(_pool, {}); }
}
}  // namespace Vulkan
}  // namespace Luna
