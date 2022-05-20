#include <vk_mem_alloc.h>

#include <Luna/Graphics/Vulkan/Buffer.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Utility/Log.hpp>

namespace Luna {
namespace Vulkan {
void BufferDeleter::operator()(Buffer* buffer) {
	buffer->_device.DestroyBuffer({}, buffer);
}

Buffer::Buffer(Device& device, const BufferCreateInfo& createInfo)
		: Cookie(device), _device(device), _createInfo(createInfo) {
	auto dev = _device.GetDevice();

	const vk::BufferCreateInfo bufferCI({}, _createInfo.Size, _createInfo.Usage, vk::SharingMode::eExclusive, nullptr);
	VmaAllocationCreateInfo bufferAI = {.usage = VMA_MEMORY_USAGE_AUTO};
	if (_createInfo.Domain == BufferDomain::Host) {
		bufferAI.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		bufferAI.preferredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}

	Log::Trace("Vulkan::Buffer", "Creating new Buffer.");

	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo allocationInfo;
	const vk::Result createResult =
		static_cast<vk::Result>(vmaCreateBuffer(_device.GetAllocator(),
	                                          reinterpret_cast<const VkBufferCreateInfo*>(&bufferCI),
	                                          &bufferAI,
	                                          &buffer,
	                                          &allocation,
	                                          &allocationInfo));
	if (createResult != vk::Result::eSuccess) {
		throw std::runtime_error(
			fmt::format("[Vulkan::Buffer] Failed to create Vulkan buffer: {}", vk::to_string(createResult)));
	}

	_buffer           = buffer;
	_allocation       = allocation;
	_mapped           = allocationInfo.pMappedData;
	_memoryProperties = _device.GetGPUInfo().Memory.memoryTypes[allocationInfo.memoryType].propertyFlags;
}

Buffer::~Buffer() noexcept {
	auto dev = _device.GetDevice();

	if (_buffer) { dev.destroyBuffer(_buffer); }
	if (_allocation) { vmaFreeMemory(_device.GetAllocator(), _allocation); }
}

void* Buffer::Map() {
	return _mapped;
}

void Buffer::Unmap() {}
}  // namespace Vulkan
}  // namespace Luna
