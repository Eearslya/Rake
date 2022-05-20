#pragma once

#include <Luna/Graphics/Vulkan/Common.hpp>

typedef struct VmaAllocator_T* VmaAllocator;

namespace Luna {
namespace Vulkan {
struct InitialImageData {
	const void* Data     = nullptr;
	uint32_t RowLength   = 0;
	uint32_t ImageHeight = 0;
};

class Device : Utility::NonCopyable {
 public:
	Device();
	~Device() noexcept;

	// Const retrieval functions.
	VmaAllocator GetAllocator() const {
		return _allocator;
	}
	const vk::Device& GetDevice() const {
		return _device;
	}
	const ExtensionInfo& GetExtensionInfo() const {
		return _extensions;
	}
	const vk::PhysicalDevice& GetGPU() const {
		return _gpu;
	}
	const GPUInfo& GetGPUInfo() const {
		return _gpuInfo;
	}
	const vk::Instance& GetInstance() const {
		return _instance;
	}
	const QueueInfo& GetQueueInfo() const {
		return _queues;
	}
	const vk::SurfaceKHR& GetSurface() const {
		return _surface;
	}

	vk::Format GetDefaultDepthFormat() const;
	vk::Format GetDefaultDepthStencilFormat() const;
	QueueType GetQueueType(CommandBufferType bufferType) const;
	RenderPassInfo GetStockRenderPass(StockRenderPass type) const;
	bool ImageFormatSupported(vk::Format format, vk::FormatFeatureFlags features, vk::ImageTiling tiling) const;

	void AddWaitSemaphore(CommandBufferType bufferType,
	                      SemaphoreHandle semaphore,
	                      vk::PipelineStageFlags stages,
	                      bool flush = false);
	void EndFrame();
	void NextFrame();
	CommandBufferHandle RequestCommandBuffer(CommandBufferType type       = CommandBufferType::Generic,
	                                         const std::string& debugName = "");
	void Submit(CommandBufferHandle& cmd,
	            FenceHandle* fence                       = nullptr,
	            std::vector<SemaphoreHandle>* semaphores = nullptr);
	void WaitIdle();

	BufferHandle CreateBuffer(const BufferCreateInfo& createInfo, const void* initialData = nullptr);
	ImageHandle CreateImage(const ImageCreateInfo& createInfo, const InitialImageData* initialData = nullptr);
	ImageViewHandle CreateImageView(const ImageViewCreateInfo& createInfo);
	DescriptorSetAllocator* RequestDescriptorSetAllocator(const DescriptorSetLayout& layout,
	                                                      const uint32_t* stagesForBindings);
	FenceHandle RequestFence();
	PipelineLayout* RequestPipelineLayout(const ProgramResourceLayout& layout);
	Program* RequestProgram(const std::string& vertexGlsl,
	                        const std::string& fragmentGlsl,
	                        const std::string& debugName = "");
	Program* RequestProgram(size_t vertCodeSize,
	                        const void* vertCode,
	                        size_t fragCodeSize,
	                        const void* fragCode,
	                        const std::string& debugName = "");
	Program* RequestProgram(Shader* vertex, Shader* fragment, const std::string& debugName = "");
	SemaphoreHandle RequestSemaphore(const std::string& debugName = "");
	Shader* RequestShader(size_t codeSize, const void* code, const std::string& debugName = "");
	Shader* RequestShader(vk::ShaderStageFlagBits stage, const std::string& glsl, const std::string& debugName = "");
	ImageHandle RequestTransientAttachment(const vk::Extent2D& extent,
	                                       vk::Format format,
	                                       uint32_t index                  = 0,
	                                       vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
	                                       uint32_t layers                 = 1) const;

	uint64_t AllocateCookie(Utility::Badge<Cookie>);
	SemaphoreHandle ConsumeReleaseSemaphore(Utility::Badge<Swapchain>);
	void DestroyBuffer(Utility::Badge<BufferDeleter>, Buffer* buffer);
	void DestroyImage(Utility::Badge<ImageDeleter>, Image* image);
	void DestroyImageView(Utility::Badge<ImageViewDeleter>, ImageView* view);
	void RecycleFence(Utility::Badge<FenceDeleter>, Fence* fence);
	void RecycleSemaphore(Utility::Badge<SemaphoreDeleter>, Semaphore* semaphore);
	void ReleaseCommandBuffer(Utility::Badge<CommandBufferDeleter>, CommandBuffer* cmdBuf);
	Framebuffer& RequestFramebuffer(Utility::Badge<CommandBuffer>, const RenderPassInfo& info);
	RenderPass& RequestRenderPass(Utility::Badge<CommandBuffer>, const RenderPassInfo& info, bool compatible = false);
	RenderPass& RequestRenderPass(Utility::Badge<FramebufferAllocator>,
	                              const RenderPassInfo& info,
	                              bool compatible = false);
	Sampler* RequestSampler(const SamplerCreateInfo& createInfo);
	Sampler* RequestSampler(StockSampler type);
	void SetAcquireSemaphore(Utility::Badge<Swapchain>, uint32_t imageIndex, SemaphoreHandle& semaphore);
	void SetupSwapchain(Utility::Badge<Swapchain>, Swapchain& swapchain);

#ifdef LUNA_VULKAN_DEBUG
	template <typename T>
	void SetObjectName(T object, const std::string& name) {
		SetObjectNameImpl(T::objectType, *reinterpret_cast<uint64_t*>(&object), name);
	}
#else
	template <typename T>
	void SetObjectName(T object, const std::string& name) {}
#endif

 private:
	// A FrameContext contains all of the information needed to complete and clean up after a frame of work.
	struct FrameContext {
		FrameContext(Device& device, uint32_t frameIndex);
		~FrameContext() noexcept;

		void Begin();

		Device& Parent;
		uint32_t FrameIndex;

		std::array<std::vector<std::unique_ptr<CommandPool>>, QueueTypeCount> CommandPools;
		std::array<std::vector<CommandBufferHandle>, QueueTypeCount> Submissions;
		std::array<uint64_t, QueueTypeCount> TimelineValues;

		std::vector<Buffer*> BuffersToDestroy;
		std::vector<vk::Fence> FencesToAwait;
		std::vector<vk::Fence> FencesToRecycle;
		std::vector<Image*> ImagesToDestroy;
		std::vector<ImageView*> ImageViewsToDestroy;
		// std::vector<Sampler*> SamplersToDestroy;
		std::vector<vk::Semaphore> SemaphoresToDestroy;
		std::vector<vk::Semaphore> SemaphoresToRecycle;
	};

	struct InitialImageBuffer {
		BufferHandle Buffer;
		std::vector<vk::BufferImageCopy> ImageCopies;
	};

	// A small structure to keep track of a "fence". An internal fence can be represented by an actual fence, or by a
	// timeline semaphore, depending on available device extensions.
	struct InternalFence {
		vk::Fence Fence;
		vk::Semaphore TimelineSemaphore;
		uint64_t TimelineValue = 0;
	};

	struct QueueData {
		std::vector<SemaphoreHandle> WaitSemaphores;
		std::vector<vk::PipelineStageFlags> WaitStages;
		bool NeedsFence = false;
		vk::Semaphore TimelineSemaphore;
		uint64_t TimelineValue = 0;
	};

	void AddWaitSemaphoreNoLock(QueueType queueType,
	                            SemaphoreHandle semaphore,
	                            vk::PipelineStageFlags stages,
	                            bool flush);
	void EndFrameNoLock();
	void FlushFrameNoLock(QueueType queueType);
	void SubmitNoLock(CommandBufferHandle cmd, FenceHandle* fence, std::vector<SemaphoreHandle>* semaphores);
	void SubmitQueue(QueueType queueType, InternalFence* submitFence, std::vector<SemaphoreHandle>* semaphores);
	void SubmitStaging(CommandBufferHandle& cmd, vk::BufferUsageFlags usage, bool flush);
	void WaitIdleNoLock();

	vk::Fence AllocateFence();
	vk::Semaphore AllocateSemaphore();
	void ReleaseFence(vk::Fence fence);
	void ReleaseSemaphore(vk::Semaphore semaphore);
	RenderPass& RequestRenderPass(const RenderPassInfo& info, bool compatible);

	FrameContext& Frame();

#ifdef LUNA_VULKAN_DEBUG
	void SetObjectNameImpl(vk::ObjectType type, uint64_t handle, const std::string& name);
#endif

	vk::DynamicLoader _loader;
	ExtensionInfo _extensions = {};
	vk::Instance _instance;
#ifdef LUNA_VULKAN_DEBUG
	vk::DebugUtilsMessengerEXT _debugMessenger;
#endif
	vk::SurfaceKHR _surface;
	GPUInfo _gpuInfo  = {};
	QueueInfo _queues = {};
	vk::PhysicalDevice _gpu;
	vk::Device _device;
	std::array<QueueData, QueueTypeCount> _queueData;

	// Our main memory allocator.
	VmaAllocator _allocator;

	// Multithreading synchronization objects.
	std::vector<vk::Fence> _availableFences;
	std::vector<vk::Semaphore> _availableSemaphores;
#ifdef LUNA_VULKAN_MT
#	ifdef TRACY_ENABLE
	TracyLockableN(std::mutex, _mutex, "Vulkan Device Lock");
#	else
	std::mutex _mutex;
#	endif
	std::atomic_uint64_t _nextCookie;
	std::condition_variable_any _pendingCommandBuffersCondition;
#else
	uint64_t _nextCookie = 0;
#endif
	uint32_t _pendingCommandBuffers = 0;

	// Swapchain/WSI Sync Objects
	SemaphoreHandle _swapchainAcquire;
	bool _swapchainAcquireConsumed = false;
	std::vector<ImageHandle> _swapchainImages;
	uint32_t _swapchainIndex;
	SemaphoreHandle _swapchainRelease;

	// Vulkan object pools.
	VulkanObjectPool<Buffer> _bufferPool;
	VulkanObjectPool<CommandBuffer> _commandBufferPool;
	VulkanObjectPool<Fence> _fencePool;
	VulkanObjectPool<Image> _imagePool;
	VulkanObjectPool<ImageView> _imageViewPool;
	VulkanObjectPool<Semaphore> _semaphorePool;

	// Vulkan hashed caches.
	VulkanCache<DescriptorSetAllocator> _descriptorSetAllocators;
	VulkanCache<PipelineLayout> _pipelineLayouts;
	VulkanCache<Program> _programs;
	VulkanCache<RenderPass> _renderPasses;
	VulkanCache<Sampler> _samplers;
	VulkanCache<Shader> _shaders;

	// Management objects.
	std::unique_ptr<FramebufferAllocator> _framebufferAllocator;
	std::unique_ptr<ShaderCompiler> _shaderCompiler;
	std::array<Sampler*, StockSamplerCount> _stockSamplers;
	std::unique_ptr<TransientAttachmentAllocator> _transientAttachmentAllocator;

	// Frame contexts.
	uint32_t _currentFrameContext = 0;
	std::vector<std::unique_ptr<FrameContext>> _frameContexts;
};
}  // namespace Vulkan
}  // namespace Luna
