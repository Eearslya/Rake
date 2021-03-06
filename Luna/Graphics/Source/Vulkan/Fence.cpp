#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/Fence.hpp>

namespace Luna {
namespace Vulkan {
void FenceDeleter::operator()(Fence* fence) {
	fence->_device.RecycleFence({}, fence);
}

Fence::Fence(Device& device, vk::Fence fence)
		: _device(device), _fence(fence), _timelineSemaphore(VK_NULL_HANDLE), _timelineValue(0) {}

Fence::Fence(Device& device, vk::Semaphore timelineSemaphore, uint64_t timelineValue)
		: _device(device), _fence(VK_NULL_HANDLE), _timelineSemaphore(timelineSemaphore), _timelineValue(timelineValue) {}

Fence::~Fence() noexcept {
	_fence             = nullptr;
	_observedWait      = false;
	_timelineSemaphore = nullptr;
	_timelineValue     = 0;
}

void Fence::Wait() {
#ifdef LUNA_VULKAN_MT
	std::lock_guard<std::mutex> lock(_mutex);
#endif

	if (_observedWait) { return; }

	auto dev = _device.GetDevice();

	if (_fence) {
		const auto waitResult = dev.waitForFences(_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		if (waitResult != vk::Result::eSuccess) { throw std::runtime_error("Failed to wait on Fence!"); }
	} else {
		assert(_timelineSemaphore && _timelineValue != 0);

		const vk::SemaphoreWaitInfo waitInfo({}, _timelineSemaphore, _timelineValue);
		const auto waitResult = dev.waitSemaphoresKHR(waitInfo, std::numeric_limits<uint64_t>::max());
		if (waitResult != vk::Result::eSuccess) { throw std::runtime_error("Failed to wait on Timeline Semaphore!"); }
	}

	_observedWait = true;
}
}  // namespace Vulkan
}  // namespace Luna
