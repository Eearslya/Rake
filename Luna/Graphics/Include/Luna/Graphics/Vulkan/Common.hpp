#pragma once

#include <Luna/Graphics/Vulkan/Cookie.hpp>
#include <Luna/Utility/Badge.hpp>
#include <Luna/Utility/EnumClass.hpp>
#include <Luna/Utility/IntrusiveHashMap.hpp>
#include <Luna/Utility/IntrusivePtr.hpp>
#include <Luna/Utility/NonCopyable.hpp>
#include <Luna/Utility/ObjectPool.hpp>
#include <Luna/Utility/TemporaryHashMap.hpp>
#include <array>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#if defined(LUNA_DEBUG) && !defined(LUNA_VULKAN_DEBUG)
#	define LUNA_VULKAN_DEBUG
#endif

// #define LUNA_VULKAN_MT

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

namespace Luna {
namespace Vulkan {
// Forward declarations.
class Buffer;
struct BufferCreateInfo;
struct BufferDeleter;
class CommandBuffer;
struct CommandBufferDeleter;
class CommandPool;
class DescriptorSet;
class DescriptorSetAllocator;
struct DescriptorSetLayout;
class Device;
class Image;
struct ImageCreateInfo;
struct ImageDeleter;
class ImageView;
struct ImageViewCreateInfo;
struct ImageViewDeleter;
class Fence;
struct FenceDeleter;
class Framebuffer;
class FramebufferAllocator;
class PipelineLayout;
class Program;
struct ProgramResourceLayout;
class RenderPass;
struct RenderPassInfo;
class Sampler;
struct SamplerCreateInfo;
class Semaphore;
struct SemaphoreDeleter;
class Shader;
class ShaderCompiler;
class Swapchain;
class TransientAttachmentAllocator;

// Handle declarations.
using BufferHandle        = Utility::IntrusivePtr<Buffer>;
using CommandBufferHandle = Utility::IntrusivePtr<CommandBuffer>;
using FenceHandle         = Utility::IntrusivePtr<Fence>;
using ImageHandle         = Utility::IntrusivePtr<Image>;
using ImageViewHandle     = Utility::IntrusivePtr<ImageView>;
using SemaphoreHandle     = Utility::IntrusivePtr<Semaphore>;

// Typedefs.
template <typename T>
using HashedObject = Utility::IntrusiveHashMapEnabled<T>;

#ifdef LUNA_VULKAN_MT
using HandleCounter = Utility::MultiThreadCounter;
template <typename T>
using VulkanCache = Utility::ThreadSafeIntrusiveHashMapReadCached<T>;
template <typename T>
using VulkanCacheReadWrite = Utility::ThreadSafeIntrusiveHashMap<T>;
template <typename T>
using VulkanObjectPool = Utility::ThreadSafeObjectPool<T>;
#else
using HandleCounter = Utility::SingleThreadCounter;
template <typename T>
using VulkanCache = Utility::IntrusiveHashMap<T>;
template <typename T>
using VulkanCacheReadWrite = Utility::IntrusiveHashMap<T>;
template <typename T>
using VulkanObjectPool = Utility::ObjectPool<T>;
#endif

// Enums and constants.
constexpr static const int DescriptorSetsPerPool      = 16;
constexpr static const int MaxColorAttachments        = 8;
constexpr static const int MaxDescriptorBindings      = 32;
constexpr static const int MaxDescriptorSets          = 4;
constexpr static const int MaxPushConstantSize        = 128;
constexpr static const int MaxSpecializationConstants = 8;
constexpr static const int MaxVertexAttributes        = 16;
constexpr static const int MaxVertexBuffers           = 8;

template <typename T>
static const char* VulkanEnumToString(const T value) {
	return nullptr;
}

enum class QueueType { Graphics, Transfer, Compute };
constexpr static const int QueueTypeCount = 3;
template <>
const char* VulkanEnumToString<QueueType>(const QueueType value) {
	switch (value) {
		case QueueType::Graphics:
			return "Graphics";
		case QueueType::Transfer:
			return "Transfer";
		case QueueType::Compute:
			return "Compute";
	}

	return "Unknown";
}

enum class CommandBufferType {
	Generic       = static_cast<int>(QueueType::Graphics),
	AsyncTransfer = static_cast<int>(QueueType::Transfer),
	AsyncCompute  = static_cast<int>(QueueType::Compute),
	AsyncGraphics = QueueTypeCount
};
constexpr static const int CommandBufferTypeCount = 4;
template <>
const char* VulkanEnumToString<CommandBufferType>(const CommandBufferType value) {
	switch (value) {
		case CommandBufferType::Generic:
			return "Generic";
		case CommandBufferType::AsyncCompute:
			return "AsyncCompute";
		case CommandBufferType::AsyncTransfer:
			return "AsyncTransfer";
		case CommandBufferType::AsyncGraphics:
			return "AsyncGraphics";
	}

	return "Unknown";
}

enum class FormatCompressionType { Uncompressed, BC, ETC, ASTC };
constexpr static const int FormatCompressionTypeCount = 4;
template <>
const char* VulkanEnumToString<FormatCompressionType>(const FormatCompressionType value) {
	switch (value) {
		case FormatCompressionType::Uncompressed:
			return "Uncompressed";
		case FormatCompressionType::BC:
			return "BC";
		case FormatCompressionType::ETC:
			return "ETC";
		case FormatCompressionType::ASTC:
			return "ASTC";
	}

	return "Unknown";
}

enum class ImageLayoutType { Optimal, General };
constexpr static const int ImageLayoutTypeCount = 2;
template <>
const char* VulkanEnumToString<ImageLayoutType>(const ImageLayoutType value) {
	switch (value) {
		case ImageLayoutType::Optimal:
			return "Optimal";
		case ImageLayoutType::General:
			return "General";
	}

	return "Unknown";
}

// Enum is made to line up with the bits in vk::ShaderStageFlagBits.
enum class ShaderStage {
	Vertex                 = 0,
	TessellationControl    = 1,
	TessellationEvaluation = 2,
	Geometry               = 3,
	Fragment               = 4,
	Compute                = 5
};
constexpr static const int ShaderStageCount = 6;
template <>
const char* VulkanEnumToString<ShaderStage>(const ShaderStage value) {
	switch (value) {
		case ShaderStage::Vertex:
			return "Vertex";
		case ShaderStage::TessellationControl:
			return "TessellationControl";
		case ShaderStage::TessellationEvaluation:
			return "TessellationEvaluation";
		case ShaderStage::Geometry:
			return "Geometry";
		case ShaderStage::Fragment:
			return "Fragment";
		case ShaderStage::Compute:
			return "Compute";
	}

	return "Unknown";
}

enum class StockRenderPass { ColorOnly, Depth, DepthStencil };
constexpr static const int StockRenderPassCount = 3;
template <>
const char* VulkanEnumToString<StockRenderPass>(const StockRenderPass value) {
	switch (value) {
		case StockRenderPass::ColorOnly:
			return "ColorOnly";
		case StockRenderPass::Depth:
			return "Depth";
		case StockRenderPass::DepthStencil:
			return "DepthStencil";
	}

	return "Unknown";
}

enum class StockSampler {
	NearestClamp,
	LinearClamp,
	TrilinearClamp,
	NearestWrap,
	LinearWrap,
	TrilinearWrap,
	NearestShadow,
	LinearShadow,
	DefaultGeometryFilterClamp,
	DefaultGeometryFilterWrap
};
constexpr static const int StockSamplerCount = 10;
template <>
const char* VulkanEnumToString<StockSampler>(const StockSampler value) {
	switch (value) {
		case StockSampler::NearestClamp:
			return "NearestClamp";
		case StockSampler::LinearClamp:
			return "LinearClamp";
		case StockSampler::TrilinearClamp:
			return "TrilinearClamp";
		case StockSampler::NearestWrap:
			return "NearestWrap";
		case StockSampler::LinearWrap:
			return "LinearWrap";
		case StockSampler::TrilinearWrap:
			return "TrilinearWrap";
		case StockSampler::NearestShadow:
			return "NearestShadow";
		case StockSampler::LinearShadow:
			return "LinearShadow";
		case StockSampler::DefaultGeometryFilterClamp:
			return "DefaultGeometryFilterClamp";
		case StockSampler::DefaultGeometryFilterWrap:
			return "DefaultGeometryFilterWrap";
	}

	return "Unknown";
}

// Structures
struct ExtensionInfo {
	bool CalibratedTimestamps         = false;
	bool DebugUtils                   = false;
	bool GetPhysicalDeviceProperties2 = false;
	bool GetSurfaceCapabilities2      = false;
	bool Maintenance1                 = false;
	bool Synchronization2             = false;
	bool TimelineSemaphore            = false;
	bool ValidationFeatures           = false;
};
struct GPUFeatures {
	vk::PhysicalDeviceFeatures Features;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	vk::PhysicalDevicePortabilitySubsetFeaturesKHR PortabilitySubset;
#endif
	vk::PhysicalDeviceSynchronization2FeaturesKHR Synchronization2;
	vk::PhysicalDeviceTimelineSemaphoreFeatures TimelineSemaphore;
};
struct GPUProperties {
	vk::PhysicalDeviceProperties Properties;
	vk::PhysicalDeviceDriverProperties Driver;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	vk::PhysicalDevicePortabilitySubsetPropertiesKHR PortabilitySubset;
#endif
	vk::PhysicalDeviceTimelineSemaphoreProperties TimelineSemaphore;
};
struct GPUInfo {
	std::vector<vk::ExtensionProperties> AvailableExtensions;
	GPUFeatures AvailableFeatures = {};
	std::vector<vk::LayerProperties> Layers;
	vk::PhysicalDeviceMemoryProperties Memory;
	GPUProperties Properties = {};
	std::vector<vk::QueueFamilyProperties> QueueFamilies;

	GPUFeatures EnabledFeatures = {};
};
struct QueueInfo {
	std::array<uint32_t, QueueTypeCount> Families;
	std::array<uint32_t, QueueTypeCount> Indices;
	std::array<vk::Queue, QueueTypeCount> Queues;

	QueueInfo() {
		std::fill(Families.begin(), Families.end(), VK_QUEUE_FAMILY_IGNORED);
		std::fill(Indices.begin(), Indices.end(), VK_QUEUE_FAMILY_IGNORED);
	}

	bool SameIndex(QueueType a, QueueType b) const {
		return Indices[static_cast<int>(a)] == Indices[static_cast<int>(b)];
	}
	bool SameFamily(QueueType a, QueueType b) const {
		return Families[static_cast<int>(a)] == Families[static_cast<int>(b)];
	}
	bool SameQueue(QueueType a, QueueType b) const {
		return Queues[static_cast<int>(a)] == Queues[static_cast<int>(b)];
	}
	std::vector<uint32_t> UniqueFamilies() const {
		std::set<uint32_t> unique;
		for (const auto& family : Families) {
			if (family != VK_QUEUE_FAMILY_IGNORED) { unique.insert(family); }
		}

		return std::vector<uint32_t>(unique.begin(), unique.end());
	}

	uint32_t& Family(QueueType type) {
		return Families[static_cast<int>(type)];
	}
	const uint32_t& Family(QueueType type) const {
		return Families[static_cast<int>(type)];
	}
	uint32_t& Index(QueueType type) {
		return Indices[static_cast<int>(type)];
	}
	const uint32_t& Index(QueueType type) const {
		return Indices[static_cast<int>(type)];
	}
	vk::Queue& Queue(QueueType type) {
		return Queues[static_cast<int>(type)];
	}
	const vk::Queue& Queue(QueueType type) const {
		return Queues[static_cast<int>(type)];
	}
};
}  // namespace Vulkan
}  // namespace Luna
