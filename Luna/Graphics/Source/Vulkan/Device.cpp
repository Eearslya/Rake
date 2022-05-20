#include <vk_mem_alloc.h>

#include <Luna/Graphics/Vulkan/Buffer.hpp>
#include <Luna/Graphics/Vulkan/CommandBuffer.hpp>
#include <Luna/Graphics/Vulkan/CommandPool.hpp>
#include <Luna/Graphics/Vulkan/DescriptorSet.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/Fence.hpp>
#include <Luna/Graphics/Vulkan/FormatLayout.hpp>
#include <Luna/Graphics/Vulkan/Image.hpp>
#include <Luna/Graphics/Vulkan/RenderPass.hpp>
#include <Luna/Graphics/Vulkan/Sampler.hpp>
#include <Luna/Graphics/Vulkan/Semaphore.hpp>
#include <Luna/Graphics/Vulkan/Shader.hpp>
#include <Luna/Graphics/Vulkan/ShaderCompiler.hpp>
#include <Luna/Graphics/Vulkan/Swapchain.hpp>
#include <Luna/Platform/Window.hpp>
#include <Luna/Utility/Log.hpp>

// Helper functions for dealing with multithreading.
#ifdef LUNA_VULKAN_MT
static uint32_t GetThreadID() {
	return ::Luna::Threading::GetThreadID();
}

// One mutex to rule them all. This might be able to be optimized to several smaller mutexes at some point.
#	define LOCK()                         \
		std::lock_guard _DeviceLock(_mutex); \
		LockMark(_mutex)

// Used for objects that can be internally synchronized. If the object has the internal sync flag, no lock is performed.
// Otherwise, the lock is made.
#	define MAYBE_LOCK(obj) \
		auto _DeviceLock = (obj->_internalSync ? std::unique_lock<decltype(_mutex)>() : std::unique_lock(_mutex))

// As we hand out command buffers on request, we keep track of how many we have given out. Once we want to finalize the
// frame and move on, we need to make sure all of the command buffers we've given out have come back to us. This
// currently allows five seconds for all command buffers to return before giving up.
#	define WAIT_FOR_PENDING_COMMAND_BUFFERS()                                                                           \
		std::unique_lock _DeviceLock(_mutex);                                                                              \
		LockMark(_mutex);                                                                                                  \
		do {                                                                                                               \
			using namespace std::chrono_literals;                                                                            \
			if (!_pendingCommandBuffersCondition.wait_for(_DeviceLock, 5s, [&]() { return _pendingCommandBuffers == 0; })) { \
				throw std::runtime_error("Timed out waiting for all requested command buffers to be submitted!");              \
			}                                                                                                                \
		} while (0)
#else
static uint32_t GetThreadID() {
	return 0;
}
#	define LOCK()          ((void) 0)
#	define MAYBE_LOCK(obj) ((void) 0)
#	define WAIT_FOR_PENDING_COMMAND_BUFFERS() \
		assert(_pendingCommandBuffers == 0 && "All command buffers must be submitted before end of frame!")
#endif

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Luna {
namespace Vulkan {
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                          VkDebugUtilsMessageTypeFlagsEXT type,
                                                          const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                          void* userData) {
	switch (severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			Log::Error("Vulkan", "Vulkan ERROR: {}", data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			Log::Warning("Vulkan", "Vulkan Warning: {}", data->pMessage);
			break;
		default:
			Log::Debug("Vulkan", "Vulkan: {}", data->pMessage);
			break;
	}

	return VK_FALSE;
}

Device::Device() {
	if (!_loader.success()) { throw std::runtime_error("Failed to load Vulkan loader!"); }

	VULKAN_HPP_DEFAULT_DISPATCHER.init(_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

	// Create Vulkan instance.
	{
		const auto requiredExtensions = Window::Get()->GetRequiredInstanceExtensions();

		struct Extension {
			std::string Name;
			uint32_t Version;
			std::string Layer;  // Layer will be an empty string if the extension is in the base layer.
		};

		const auto availableLayers = vk::enumerateInstanceLayerProperties();
		std::unordered_map<std::string, Extension> availableExtensions;
		std::vector<const char*> enabledExtensions;
		std::vector<const char*> enabledLayers;

		// Find all of our instance extensions. This will look through all of the available layers, and if an extension is
		// available in multiple ways, it will prefer whatever layer has the highest spec version.
		{
			const auto EnumerateExtensions = [&](const vk::LayerProperties* layer) -> void {
				std::vector<vk::ExtensionProperties> extensions;
				if (layer == nullptr) {
					extensions = vk::enumerateInstanceExtensionProperties(nullptr);
				} else {
					extensions = vk::enumerateInstanceExtensionProperties(std::string(layer->layerName));
				}

				for (const auto& extension : extensions) {
					const std::string name = std::string(extension.extensionName);
					Extension ext{name, extension.specVersion, layer ? std::string(layer->layerName) : ""};
					auto it = availableExtensions.find(name);
					if (it == availableExtensions.end() || it->second.Version < ext.Version) { availableExtensions[name] = ext; }
				}
			};
			EnumerateExtensions(nullptr);
			for (const auto& layer : availableLayers) { EnumerateExtensions(&layer); }
		}

		// Enable all of the required extensions and a handful of preferred extensions or layers. If we can't find any of
		// the required extensions, we fail.
		{
			const auto HasLayer = [&availableLayers](const char* layerName) -> bool {
				for (const auto& layer : availableLayers) {
					if (strcmp(layer.layerName, layerName) == 0) { return true; }
				}

				return false;
			};
			const auto TryLayer = [&](const char* layerName) -> bool {
				if (!HasLayer(layerName)) { return false; }
				for (const auto& name : enabledLayers) {
					if (strcmp(name, layerName) == 0) { return true; }
				}
				Log::Debug("Vulkan::Device", "Enabling instance layer '{}'.", layerName);
				enabledLayers.push_back(layerName);
				return true;
			};
			const auto HasExtension = [&availableExtensions](const char* extensionName) -> bool {
				const std::string name(extensionName);
				const auto it = availableExtensions.find(name);
				return it != availableExtensions.end();
			};
			const auto TryExtension = [&](const char* extensionName) -> bool {
				for (const auto& name : enabledExtensions) {
					if (strcmp(name, extensionName) == 0) { return true; }
				}
				if (!HasExtension(extensionName)) { return false; }
				const std::string name(extensionName);
				const auto it = availableExtensions.find(name);
				if (it->second.Layer.length() != 0) { TryLayer(it->second.Layer.c_str()); }
				Log::Debug("Vulkan::Device", "Enabling instance extension '{}'.", extensionName);
				enabledExtensions.push_back(extensionName);
				return true;
			};

			for (const auto& ext : requiredExtensions) {
				if (!TryExtension(ext)) {
					throw vk::ExtensionNotPresentError("Extension " + std::string(ext) + " was not available!");
				}
			}

			// If we can't get the real sync2, see if we can get the fake compatibility extension.
			TryLayer("VK_LAYER_KHRONOS_synchronization2");

			_extensions.GetPhysicalDeviceProperties2 = TryExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
			_extensions.GetSurfaceCapabilities2      = TryExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

#ifdef LUNA_VULKAN_DEBUG
			TryLayer("VK_LAYER_KHRONOS_validation");

			_extensions.DebugUtils         = TryExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			_extensions.ValidationFeatures = TryExtension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
#endif
		}

		const vk::ApplicationInfo appInfo(
			"Luna", VK_MAKE_API_VERSION(0, 1, 0, 0), "Luna", VK_MAKE_API_VERSION(0, 1, 0, 0), VK_API_VERSION_1_0);
		const vk::InstanceCreateInfo instanceCI({}, &appInfo, enabledLayers, enabledExtensions);

#ifdef LUNA_VULKAN_DEBUG
		const vk::DebugUtilsMessengerCreateInfoEXT debugCI(
			{},
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
			VulkanDebugCallback,
			this);
		const std::vector<vk::ValidationFeatureEnableEXT> validationEnable = {
			vk::ValidationFeatureEnableEXT::eBestPractices, vk::ValidationFeatureEnableEXT::eSynchronizationValidation};
		const std::vector<vk::ValidationFeatureDisableEXT> validationDisable;
		const vk::ValidationFeaturesEXT validationCI(validationEnable, validationDisable);

		vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT, vk::ValidationFeaturesEXT> chain(
			instanceCI, debugCI, validationCI);

		if (!_extensions.DebugUtils) { chain.unlink<vk::DebugUtilsMessengerCreateInfoEXT>(); }
		if (!_extensions.ValidationFeatures) { chain.unlink<vk::ValidationFeaturesEXT>(); }

		_instance = vk::createInstance(chain.get());
#else
		_instance = vk::createInstance(instanceCI);
#endif

		Log::Trace("Vulkan::Device", "Instance created.");

		VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

		Log::Trace("Vulkan::Device", "Instance functions loaded.");

#ifdef LUNA_VULKAN_DEBUG
		if (_extensions.DebugUtils) {
			_debugMessenger = _instance.createDebugUtilsMessengerEXT(debugCI);
			Log::Trace("Vulkan::Device", "Debug Messenger created.");
		}
#endif

		_surface = Window::Get()->CreateSurface(_instance);
	}

	// Select Physical Device.
	const std::vector<const char*> requiredDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	{
		const auto gpus = _instance.enumeratePhysicalDevices();
		for (const auto& gpu : gpus) {
			GPUInfo gpuInfo;

			// Enumerate basic information.
			gpuInfo.AvailableExtensions = gpu.enumerateDeviceExtensionProperties(nullptr);
			gpuInfo.Layers              = gpu.enumerateDeviceLayerProperties();
			gpuInfo.Memory              = gpu.getMemoryProperties();
			gpuInfo.QueueFamilies       = gpu.getQueueFamilyProperties();

			// Find any extensions hidden within enabled layers.
			for (const auto& layer : gpuInfo.Layers) {
				const auto layerExtensions = gpu.enumerateDeviceExtensionProperties(std::string(layer.layerName));
				for (const auto& ext : layerExtensions) {
					auto it = std::find_if(gpuInfo.AvailableExtensions.begin(),
					                       gpuInfo.AvailableExtensions.end(),
					                       [ext](const vk::ExtensionProperties& props) -> bool {
																	 return strcmp(props.extensionName, ext.extensionName) == 0;
																 });
					if (it == gpuInfo.AvailableExtensions.end()) {
						gpuInfo.AvailableExtensions.push_back(ext);
					} else if (it->specVersion < ext.specVersion) {
						it->specVersion = ext.specVersion;
					}
				}
			}

			const auto HasExtension = [&](const char* extensionName) -> bool {
				return std::find_if(gpuInfo.AvailableExtensions.begin(),
				                    gpuInfo.AvailableExtensions.end(),
				                    [extensionName](const vk::ExtensionProperties& props) -> bool {
															return strcmp(extensionName, props.extensionName) == 0;
														}) != gpuInfo.AvailableExtensions.end();
			};

			// Enumerate all of the properties and features.
			if (_extensions.GetPhysicalDeviceProperties2) {
				vk::StructureChain<vk::PhysicalDeviceFeatures2,
#ifdef VK_ENABLE_BETA_EXTENSIONS
				                   vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
#endif
				                   vk::PhysicalDeviceSynchronization2FeaturesKHR,
				                   vk::PhysicalDeviceTimelineSemaphoreFeatures>
					features;
				vk::StructureChain<vk::PhysicalDeviceProperties2,
				                   vk::PhysicalDeviceDriverProperties,
#ifdef VK_ENABLE_BETA_EXTENSIONS
				                   vk::PhysicalDevicePortabilitySubsetPropertiesKHR,
#endif
				                   vk::PhysicalDeviceTimelineSemaphoreProperties>
					properties;

				if (!HasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
					properties.unlink<vk::PhysicalDeviceDriverProperties>();
				}
#ifdef VK_ENABLE_BETA_EXTENSIONS
				if (!HasExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
					features.unlink<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
					properties.unlink<vk::PhysicalDevicePortabilitySubsetPropertiesKHR>();
				}
#endif
				if (!HasExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
					features.unlink<vk::PhysicalDeviceSynchronization2FeaturesKHR>();
				}
				if (!HasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
					features.unlink<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
					properties.unlink<vk::PhysicalDeviceTimelineSemaphoreProperties>();
				}

				gpu.getFeatures2KHR(&features.get());
				gpu.getProperties2KHR(&properties.get());

				gpuInfo.AvailableFeatures.Features = features.get().features;
#ifdef VK_ENABLE_BETA_EXTENSIONS
				gpuInfo.AvailableFeatures.PortabilitySubset = features.get<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
#endif
				gpuInfo.AvailableFeatures.Synchronization2  = features.get<vk::PhysicalDeviceSynchronization2FeaturesKHR>();
				gpuInfo.AvailableFeatures.TimelineSemaphore = features.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();

				gpuInfo.Properties.Properties = properties.get().properties;
				gpuInfo.Properties.Driver     = properties.get<vk::PhysicalDeviceDriverProperties>();
#ifdef VK_ENABLE_BETA_EXTENSIONS
				gpuInfo.Properties.PortabilitySubset = properties.get<vk::PhysicalDevicePortabilitySubsetPropertiesKHR>();
#endif
				gpuInfo.Properties.TimelineSemaphore = properties.get<vk::PhysicalDeviceTimelineSemaphoreProperties>();
			} else {
				gpuInfo.AvailableFeatures.Features = gpu.getFeatures();
				gpuInfo.Properties.Properties      = gpu.getProperties();
			}

			// Validate that the device meets requirements.
			bool extensions = true;
			for (const auto& ext : requiredDeviceExtensions) {
				if (!HasExtension(ext)) {
					Log::Trace("Vulkan::Device", "Candidate device is missing required extension: {}", ext);
					extensions = false;
					break;
				}
			}
			if (!extensions) {
				Log::Trace("Vulkan::Device", "Rejecting physical device: Missing extensions.");
				continue;
			}

			bool graphicsQueue = false;
			for (size_t q = 0; q < gpuInfo.QueueFamilies.size(); ++q) {
				const auto& family = gpuInfo.QueueFamilies[q];
				const auto present = gpu.getSurfaceSupportKHR(q, _surface);
				if (family.queueFlags & vk::QueueFlagBits::eGraphics && family.queueFlags & vk::QueueFlagBits::eCompute &&
				    present) {
					graphicsQueue = true;
					break;
				}
			}
			if (!graphicsQueue) {
				Log::Trace("Vulkan::Device", "Rejecting physical device: Missing graphics queue.");
				continue;
			}

			// We have a winner!
			_gpu     = gpu;
			_gpuInfo = gpuInfo;
			break;
		}

		if (!_gpu) { throw std::runtime_error("Failed to find a compatible physical device!"); }
	}

	// Create Logical device.
	{
		std::vector<const char*> enabledExtensions;
		// Find and enable all required extensions, and any extensions we would like to have but are not required.
		{
			const auto HasExtension = [&](const char* extensionName) -> bool {
				for (const auto& ext : _gpuInfo.AvailableExtensions) {
					if (strcmp(ext.extensionName, extensionName) == 0) { return true; }
				}
				return false;
			};
			const auto TryExtension = [&](const char* extensionName) -> bool {
				if (!HasExtension(extensionName)) { return false; }
				for (const auto& name : enabledExtensions) {
					if (strcmp(name, extensionName) == 0) { return true; }
				}
				Log::Debug("Vulkan::Device", "Enabling device extension '{}'.", extensionName);
				enabledExtensions.push_back(extensionName);

				return true;
			};
			for (const auto& name : requiredDeviceExtensions) {
				if (!TryExtension(name)) {
					throw vk::ExtensionNotPresentError("Extension " + std::string(name) + " was not available!");
				}
			}

#ifdef VK_ENABLE_BETA_EXTENSIONS
			// If this extension is available, it is REQUIRED to enable.
			TryExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

			_extensions.CalibratedTimestamps = TryExtension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
			_extensions.Maintenance1         = TryExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
			_extensions.Synchronization2     = TryExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
			_extensions.TimelineSemaphore    = TryExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
		}

		// Find and assign all of our queues.
		auto familyProps = _gpuInfo.QueueFamilies;
		std::vector<std::vector<float>> familyPriorities(familyProps.size());
		std::vector<vk::DeviceQueueCreateInfo> queueCIs(QueueTypeCount);
		{
			std::vector<uint32_t> nextFamilyIndex(familyProps.size(), 0);

			// Assign each of our Graphics, Compute, and Transfer queues. Prefer finding separate queues for
			// each if at all possible.
			const auto AssignQueue = [&](QueueType type, vk::QueueFlags required, vk::QueueFlags ignored) -> bool {
				for (size_t q = 0; q < familyProps.size(); ++q) {
					auto& family = familyProps[q];
					if ((family.queueFlags & required) != required) { continue; }
					if (family.queueFlags & ignored || family.queueCount == 0) { continue; }

					// Require presentation support for our graphics queue.
					if (type == QueueType::Graphics) {
						const auto present = _gpu.getSurfaceSupportKHR(q, _surface);
						if (!present) { continue; }
					}

					_queues.Family(type) = q;
					_queues.Index(type)  = nextFamilyIndex[q]++;
					--family.queueCount;
					familyPriorities[q].push_back(1.0f);

					Log::Debug("Vulkan::Device",
					           "Using queue {}.{} for {}.",
					           _queues.Family(type),
					           _queues.Index(type),
					           VulkanEnumToString(type));

					return true;
				}

				return false;
			};

			// First find our main Graphics queue.
			if (!AssignQueue(QueueType::Graphics, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute, {})) {
				throw vk::IncompatibleDriverError("Could not find a suitable graphics/compute queue!");
			}

			// Then, attempt to find a dedicated compute queue, or any unused queue with compute. Fall back
			// to sharing with graphics.
			if (!AssignQueue(QueueType::Compute, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics) &&
			    !AssignQueue(QueueType::Compute, vk::QueueFlagBits::eCompute, {})) {
				_queues.Family(QueueType::Compute) = _queues.Family(QueueType::Graphics);
				_queues.Index(QueueType::Compute)  = _queues.Index(QueueType::Graphics);
				Log::Debug("Vulkan::Device", "Sharing Compute queue with Graphics.");
			}

			// Finally, attempt to find a dedicated transfer queue. Try to avoid graphics/compute, then
			// compute, then just take what we can. Fall back to sharing with compute.
			if (!AssignQueue(QueueType::Transfer,
			                 vk::QueueFlagBits::eTransfer,
			                 vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute) &&
			    !AssignQueue(QueueType::Transfer, vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eCompute) &&
			    !AssignQueue(QueueType::Transfer, vk::QueueFlagBits::eTransfer, {})) {
				_queues.Family(QueueType::Transfer) = _queues.Family(QueueType::Compute);
				_queues.Index(QueueType::Transfer)  = _queues.Index(QueueType::Compute);
				Log::Debug("Vulkan::Device", "Sharing Transfer queue with Compute.");
			}

			uint32_t familyCount = 0;
			uint32_t queueCount  = 0;
			for (uint32_t i = 0; i < familyProps.size(); ++i) {
				if (nextFamilyIndex[i] > 0) {
					queueCount += nextFamilyIndex[i];
					queueCIs[familyCount++] = vk::DeviceQueueCreateInfo({}, i, nextFamilyIndex[i], familyPriorities[i].data());
				}
			}
			queueCIs.resize(familyCount);
			Log::Trace("Vulkan::Device", "Creating {} queues on {} unique families.", queueCount, familyCount);
		}

		// Determine what features we want to enable.
		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceTimelineSemaphoreFeatures> enabledFeaturesChain;
		{
			auto& features = enabledFeaturesChain.get<vk::PhysicalDeviceFeatures2>().features;
			if (_gpuInfo.AvailableFeatures.Features.samplerAnisotropy == VK_TRUE) {
				Log::Trace("Vulkan::Device",
				           "Enabling Sampler Anisotropy (x{}).",
				           _gpuInfo.Properties.Properties.limits.maxSamplerAnisotropy);
				features.samplerAnisotropy = VK_TRUE;
			}
			if (_gpuInfo.AvailableFeatures.Features.depthClamp == VK_TRUE) {
				Log::Trace("Vulkan::Device", "Enabling Depth Clamp.");
				features.depthClamp = VK_TRUE;
			}

			if (_extensions.TimelineSemaphore) {
				auto& timelineSemaphore = enabledFeaturesChain.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
				if (_gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore == VK_TRUE) {
					Log::Trace("Vulkan::Device", "Enabling Timeline Semaphores.");
					timelineSemaphore.timelineSemaphore = VK_TRUE;
				}
			} else {
				enabledFeaturesChain.unlink<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
			}

			_gpuInfo.EnabledFeatures.Features = features;
			_gpuInfo.EnabledFeatures.TimelineSemaphore =
				enabledFeaturesChain.get<vk::PhysicalDeviceTimelineSemaphoreFeatures>();
		}

		// Create our device.
		const vk::DeviceCreateInfo deviceCI(
			{},
			queueCIs,
			nullptr,
			enabledExtensions,
			_extensions.GetPhysicalDeviceProperties2 ? nullptr : &_gpuInfo.EnabledFeatures.Features);
		vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2> chain(deviceCI, enabledFeaturesChain.get());
		if (!_extensions.GetPhysicalDeviceProperties2) { chain.unlink<vk::PhysicalDeviceFeatures2>(); }

		_device = _gpu.createDevice(chain.get());
		Log::Trace("Vulkan::Device", "Device created.");
		SetObjectName(_device, "Logical Device");

		VULKAN_HPP_DEFAULT_DISPATCHER.init(_device);
		Log::Trace("Vulkan::Device", "Device functions loaded.");

		// Fetch our created queues.
		for (int q = 0; q < QueueTypeCount; ++q) {
			if (_queues.Families[q] != VK_QUEUE_FAMILY_IGNORED && _queues.Indices[q] != VK_QUEUE_FAMILY_IGNORED) {
				_queues.Queues[q] = _device.getQueue(_queues.Families[q], _queues.Indices[q]);
				std::vector<std::string> queueTypes;
				if (_queues.Family(QueueType::Graphics) == _queues.Families[q] &&
				    _queues.Index(QueueType::Graphics) == _queues.Indices[q]) {
					queueTypes.push_back("Graphics");
				}
				if (_queues.Family(QueueType::Transfer) == _queues.Families[q] &&
				    _queues.Index(QueueType::Transfer) == _queues.Indices[q]) {
					queueTypes.push_back("Transfer");
				}
				if (_queues.Family(QueueType::Compute) == _queues.Families[q] &&
				    _queues.Index(QueueType::Compute) == _queues.Indices[q]) {
					queueTypes.push_back("Compute");
				}
				const std::string combinedTitle = fmt::format("{} Queue", fmt::join(queueTypes, "/"));
				SetObjectName(_queues.Queues[q], combinedTitle);
			}
		}
	}

	// Create memory allocator.
	{
#define FN(name) .name = VULKAN_HPP_DEFAULT_DISPATCHER.name
		const VmaVulkanFunctions vmaFunctions = {FN(vkGetInstanceProcAddr),
		                                         FN(vkGetDeviceProcAddr),
		                                         FN(vkGetPhysicalDeviceProperties),
		                                         FN(vkGetPhysicalDeviceMemoryProperties),
		                                         FN(vkAllocateMemory),
		                                         FN(vkFreeMemory),
		                                         FN(vkMapMemory),
		                                         FN(vkUnmapMemory),
		                                         FN(vkFlushMappedMemoryRanges),
		                                         FN(vkInvalidateMappedMemoryRanges),
		                                         FN(vkBindBufferMemory),
		                                         FN(vkBindImageMemory),
		                                         FN(vkGetBufferMemoryRequirements),
		                                         FN(vkGetImageMemoryRequirements),
		                                         FN(vkCreateBuffer),
		                                         FN(vkDestroyBuffer),
		                                         FN(vkCreateImage),
		                                         FN(vkDestroyImage),
		                                         FN(vkCmdCopyBuffer)};
#undef FN
		const VmaAllocatorCreateInfo allocatorCI = {.physicalDevice   = _gpu,
		                                            .device           = _device,
		                                            .pVulkanFunctions = &vmaFunctions,
		                                            .instance         = _instance,
		                                            .vulkanApiVersion = VK_API_VERSION_1_0};
		const auto allocatorResult               = vmaCreateAllocator(&allocatorCI, &_allocator);
		if (allocatorResult != VK_SUCCESS) {
			throw std::runtime_error("[Vulkan::Device] Failed to create memory allocator!");
		}
	}

	// Create frame contexts.
	{
		_currentFrameContext = 0;
		for (uint32_t i = 0; i < 2; ++i) { _frameContexts.emplace_back(std::make_unique<FrameContext>(*this, i)); }
	}

	// Create timeline semaphores.
	{
		if (_gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore) {
			const vk::SemaphoreCreateInfo semaphoreCI;
			const vk::SemaphoreTypeCreateInfo semaphoreType(vk::SemaphoreType::eTimeline, 0);
			const vk::StructureChain chain(semaphoreCI, semaphoreType);
			unsigned int q = 0;
			for (auto& queue : _queueData) {
				queue.TimelineSemaphore = _device.createSemaphore(chain.get());
				queue.TimelineValue     = 0;

				Log::Trace("Vulkan::Device", "Timeline Semaphore created.");

				SetObjectName(queue.TimelineSemaphore,
				              fmt::format("{} Timeline Semaphore", VulkanEnumToString(static_cast<QueueType>(q))));

				q++;
			}
		}
	}

	// Initialize our helper classes.
	{
		_framebufferAllocator         = std::make_unique<FramebufferAllocator>(*this);
		_shaderCompiler               = std::make_unique<ShaderCompiler>();
		_transientAttachmentAllocator = std::make_unique<TransientAttachmentAllocator>(*this);
	}

	// Create stock samplers.
	{
		for (int i = 0; i < StockSamplerCount; ++i) {
			const auto type = static_cast<StockSampler>(i);
			SamplerCreateInfo info{};
			info.MinLod = 0.0f;
			info.MaxLod = 8.0f;

			if (type == StockSampler::DefaultGeometryFilterClamp || type == StockSampler::DefaultGeometryFilterWrap ||
			    type == StockSampler::LinearClamp || type == StockSampler::LinearShadow || type == StockSampler::LinearWrap ||
			    type == StockSampler::TrilinearClamp || type == StockSampler::TrilinearWrap) {
				info.MagFilter = vk::Filter::eLinear;
				info.MinFilter = vk::Filter::eLinear;
			}

			if (type == StockSampler::DefaultGeometryFilterClamp || type == StockSampler::DefaultGeometryFilterWrap ||
			    type == StockSampler::TrilinearClamp || type == StockSampler::TrilinearWrap) {
				info.MipmapMode = vk::SamplerMipmapMode::eLinear;
			}

			if (type == StockSampler::DefaultGeometryFilterClamp || type == StockSampler::LinearClamp ||
			    type == StockSampler::LinearShadow || type == StockSampler::NearestClamp ||
			    type == StockSampler::NearestShadow || type == StockSampler::TrilinearClamp) {
				info.AddressModeU = vk::SamplerAddressMode::eClampToEdge;
				info.AddressModeV = vk::SamplerAddressMode::eClampToEdge;
				info.AddressModeW = vk::SamplerAddressMode::eClampToEdge;
			}

			if (type == StockSampler::DefaultGeometryFilterClamp || type == StockSampler::DefaultGeometryFilterWrap) {
				if (_gpuInfo.EnabledFeatures.Features.samplerAnisotropy) {
					info.AnisotropyEnable = VK_TRUE;
					info.MaxAnisotropy    = std::min(_gpuInfo.Properties.Properties.limits.maxSamplerAnisotropy, 16.0f);
				}
			}

			if (type == StockSampler::LinearShadow || type == StockSampler::NearestShadow) {
				info.CompareEnable = VK_TRUE;
				info.CompareOp     = vk::CompareOp::eLessOrEqual;
			}

			_stockSamplers[i] = RequestSampler(info);

			SetObjectName(_stockSamplers[i]->GetSampler(),
			              fmt::format("{} Stock Sampler", VulkanEnumToString(static_cast<StockSampler>(type))));
		}
	}
}

Device::~Device() noexcept {
	WaitIdle();

	// Destroy our pooled sync objects.
	for (auto& fence : _availableFences) { _device.destroyFence(fence); }
	for (auto& semaphore : _availableSemaphores) { _device.destroySemaphore(semaphore); }

	// Reset all of our WSI synchronization.
	_swapchainAcquire.Reset();
	_swapchainRelease.Reset();
	_swapchainImages.clear();

	// Reset all of our helper classes.
	_framebufferAllocator.reset();
	_shaderCompiler.reset();
	_transientAttachmentAllocator.reset();

	for (auto& queue : _queueData) {
		if (queue.TimelineSemaphore) {
			_device.destroySemaphore(queue.TimelineSemaphore);
			queue.TimelineSemaphore = nullptr;
		}
	}

	// Clean up our VMA allocator.
	vmaDestroyAllocator(_allocator);

	WaitIdle();

	_frameContexts.clear();
	_descriptorSetAllocators.Clear();
	_pipelineLayouts.Clear();
	_programs.Clear();
	_renderPasses.Clear();
	_samplers.Clear();
	_shaders.Clear();

	if (_device) {
		_device.waitIdle();
		_device.destroy();
	}
	if (_instance) {
		if (_surface) { _instance.destroySurfaceKHR(_surface); }
#ifdef LUNA_VULKAN_DEBUG
		if (_debugMessenger) { _instance.destroyDebugUtilsMessengerEXT(_debugMessenger); }
#endif
		_instance.destroy();
	}
}

vk::Format Device::GetDefaultDepthFormat() const {
	if (ImageFormatSupported(
				vk::Format::eD32Sfloat, vk::FormatFeatureFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal)) {
		return vk::Format::eD32Sfloat;
	}
	if (ImageFormatSupported(
				vk::Format::eX8D24UnormPack32, vk::FormatFeatureFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal)) {
		return vk::Format::eX8D24UnormPack32;
	}
	if (ImageFormatSupported(
				vk::Format::eD16Unorm, vk::FormatFeatureFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal)) {
		return vk::Format::eD16Unorm;
	}

	return vk::Format::eUndefined;
}

vk::Format Device::GetDefaultDepthStencilFormat() const {
	if (ImageFormatSupported(
				vk::Format::eD24UnormS8Uint, vk::FormatFeatureFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal)) {
		return vk::Format::eD24UnormS8Uint;
	}
	if (ImageFormatSupported(
				vk::Format::eD32SfloatS8Uint, vk::FormatFeatureFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal)) {
		return vk::Format::eD32SfloatS8Uint;
	}

	return vk::Format::eUndefined;
}

// Helper function to determine the physical queue type to use for a command buffer.
QueueType Device::GetQueueType(CommandBufferType bufferType) const {
	if (bufferType == CommandBufferType::AsyncGraphics) {
		// For async graphics, if our graphics and compute queues are the same family, but different queues, we give the
		// compute queue. Otherwise, stick with the graphics queue.
		if (_queues.SameFamily(QueueType::Graphics, QueueType::Compute) &&
		    !_queues.SameIndex(QueueType::Graphics, QueueType::Compute)) {
			return QueueType::Compute;
		} else {
			return QueueType::Graphics;
		}
	}

	// For everything else, the CommandBufferType enum has the same values as the QueueType enum already.
	return static_cast<QueueType>(bufferType);
}

RenderPassInfo Device::GetStockRenderPass(StockRenderPass type) const {
	RenderPassInfo info{.ColorAttachmentCount = 1, .ClearAttachments = 1, .StoreAttachments = 1};
	info.ColorAttachments[0] = _swapchainImages[_swapchainIndex]->GetView().Get();
	info.Name                = "Color-Only Swapchain Pass";

	switch (type) {
		case StockRenderPass::Depth: {
			info.DSOps |= DepthStencilOpBits::ClearDepthStencil;
			auto depth = RequestTransientAttachment(_swapchainImages[_swapchainIndex]->GetExtent(), GetDefaultDepthFormat());
			info.DepthStencilAttachment = &(*depth->GetView());
			info.Name                   = "Depth Swapchain Pass";
			break;
		}

		case StockRenderPass::DepthStencil: {
			info.DSOps |= DepthStencilOpBits::ClearDepthStencil;
			auto depthStencil =
				RequestTransientAttachment(_swapchainImages[_swapchainIndex]->GetExtent(), GetDefaultDepthStencilFormat());
			info.DepthStencilAttachment = &(*depthStencil->GetView());
			info.Name                   = "Depth/Stencil Swapchain Pass";
			break;
		}

		default:
			break;
	}

	return info;
}

bool Device::ImageFormatSupported(vk::Format format, vk::FormatFeatureFlags features, vk::ImageTiling tiling) const {
	const auto props = _gpu.getFormatProperties(format);
	if (tiling == vk::ImageTiling::eOptimal) {
		return (props.optimalTilingFeatures & features) == features;
	} else {
		return (props.linearTilingFeatures & features) == features;
	}
}

// Add a semaphore that needs to be waited on for the specified command buffer's queue.
void Device::AddWaitSemaphore(CommandBufferType bufferType,
                              SemaphoreHandle semaphore,
                              vk::PipelineStageFlags stages,
                              bool flush) {
	LOCK();
	AddWaitSemaphoreNoLock(GetQueueType(bufferType), semaphore, stages, flush);
}

void Device::EndFrame() {
	WAIT_FOR_PENDING_COMMAND_BUFFERS();
	EndFrameNoLock();
}

void Device::NextFrame() {
	WAIT_FOR_PENDING_COMMAND_BUFFERS();

	EndFrameNoLock();

	_framebufferAllocator->BeginFrame();
	_transientAttachmentAllocator->BeginFrame();

	_currentFrameContext = (_currentFrameContext + 1) % _frameContexts.size();
	Frame().Begin();
}

// Request a command buffer from the specified queue. The returned command buffer will be started and ready to record
// immediately.
CommandBufferHandle Device::RequestCommandBuffer(CommandBufferType type, const std::string& debugName) {
	CommandBufferHandle handle;

	const auto threadIndex = GetThreadID();
	const auto queueType   = GetQueueType(type);
	auto& pool             = Frame().CommandPools[static_cast<int>(queueType)][threadIndex];
	vk::CommandBuffer buffer;
	{
		LOCK();
		buffer = pool->RequestCommandBuffer();
		_pendingCommandBuffers++;
	}

	if (debugName.length() > 0) { SetObjectName(buffer, debugName); }

	handle = CommandBufferHandle(_commandBufferPool.Allocate(*this, buffer, type, threadIndex));
	handle->Begin();

	return handle;
}

// Submit a command buffer for processing. All command buffers retrieved from the device must be submitted on the same
// frame.
void Device::Submit(CommandBufferHandle& cmd, FenceHandle* fence, std::vector<SemaphoreHandle>* semaphores) {
	LOCK();
	SubmitNoLock(std::move(cmd), fence, semaphores);
}

void Device::WaitIdle() {
	WAIT_FOR_PENDING_COMMAND_BUFFERS();
	WaitIdleNoLock();
}

BufferHandle Device::CreateBuffer(const BufferCreateInfo& createInfo, const void* initialData) {
	BufferCreateInfo actualInfo = createInfo;
	if (createInfo.Domain == BufferDomain::Device && initialData) {
		actualInfo.Usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	auto handle = BufferHandle(_bufferPool.Allocate(*this, actualInfo));

	if (createInfo.Domain == BufferDomain::Device && initialData && !handle->CanMap()) {
		auto stagingInfo   = createInfo;
		stagingInfo.Domain = BufferDomain::Host;
		stagingInfo.Usage |= vk::BufferUsageFlagBits::eTransferSrc;
		auto stagingBuffer = CreateBuffer(stagingInfo, initialData);

		auto transferCmd = RequestCommandBuffer(CommandBufferType::AsyncTransfer);
		transferCmd->CopyBuffer(*handle, *stagingBuffer);

		{
			LOCK();
			SubmitStaging(transferCmd, actualInfo.Usage, true);
		}
	} else if (initialData) {
		void* data = handle->Map();
		if (data) {
			memcpy(data, initialData, createInfo.Size);
			handle->Unmap();
		} else {
			Log::Error("Vulkan::Device", "Failed to map buffer!");
		}
	}

	return handle;
}

ImageHandle Device::CreateImage(const ImageCreateInfo& createInfo, const InitialImageData* initialData) {
	const auto formatFeatures = _gpu.getFormatProperties(createInfo.Format);

	// If we have image data to put into this image, we first prepare a staging buffer.
	InitialImageBuffer initialBuffer;
	if (initialData) {
		const bool generateMips = createInfo.Flags & ImageCreateFlagBits::GenerateMipmaps;

		// If we want to load mipmaps, we first need to determine how many levels there will be. If we are generating
		// mipmaps, we only have one mip to load anyway.
		uint32_t copyLevels = createInfo.MipLevels;
		if (generateMips) {
			copyLevels = 1;
		} else if (createInfo.MipLevels == 0) {
			copyLevels = CalculateMipLevels(createInfo.Extent);
		}

		// Next, we need our helper class initialized with our image layout to help with calculating mip offsets.
		FormatLayout layout;
		switch (createInfo.Type) {
			case vk::ImageType::e1D:
				layout = FormatLayout(createInfo.Format, createInfo.Extent.width, createInfo.ArrayLayers, copyLevels);
				break;
			case vk::ImageType::e2D:
				layout = FormatLayout(createInfo.Format,
				                      vk::Extent2D(createInfo.Extent.width, createInfo.Extent.height),
				                      createInfo.ArrayLayers,
				                      copyLevels);
				break;
			case vk::ImageType::e3D:
				layout = FormatLayout(createInfo.Format, createInfo.Extent, copyLevels);
				break;
			default:
				return {};
		}

		// Now we create the actual staging buffer.
		const BufferCreateInfo bufferCI(BufferDomain::Host,
		                                layout.GetRequiredSize(),
		                                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
		initialBuffer.Buffer = CreateBuffer(bufferCI);

		// Map our data and set our layout class up.
		void* data     = initialBuffer.Buffer->Map();
		uint32_t index = 0;
		layout.SetBuffer(data, bufferCI.Size);

		// Now we do the copies of each mip level and array layer.
		for (uint32_t level = 0; level < copyLevels; ++level) {
			const auto& mipInfo          = layout.GetMipInfo(level);
			const size_t dstHeightStride = layout.GetLayerSize(level);
			const size_t rowSize         = layout.GetRowSize(level);

			for (uint32_t layer = 0; layer < createInfo.ArrayLayers; ++layer, ++index) {
				const uint32_t srcRowLength = initialData[index].RowLength ? initialData[index].RowLength : mipInfo.RowLength;
				const uint32_t srcArrayHeight =
					initialData[index].ImageHeight ? initialData[index].ImageHeight : mipInfo.ImageHeight;
				const uint32_t srcRowStride    = layout.RowByteStride(srcRowLength);
				const uint32_t srcHeightStride = layout.LayerByteStride(srcArrayHeight, srcRowStride);

				uint8_t* dst       = static_cast<uint8_t*>(layout.Data(layer, level));
				const uint8_t* src = static_cast<const uint8_t*>(initialData[index].Data);

				for (uint32_t z = 0; z < mipInfo.Extent.depth; ++z) {
					for (uint32_t y = 0; y < mipInfo.Extent.height; ++y) {
						memcpy(
							dst + (z * dstHeightStride) + (y * rowSize), src + (z * srcHeightStride) + (y * srcRowStride), rowSize);
					}
				}
			}
		}

		initialBuffer.Buffer->Unmap();
		initialBuffer.ImageCopies = layout.BuildBufferImageCopies();
	}

	// Verify whether this format is allowed to be used for mipmap generation.
	bool generateMips                        = bool(createInfo.Flags & ImageCreateFlagBits::GenerateMipmaps);
	const auto requiredMipGenerationFeatures = vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc;
	if (generateMips &&
	    (formatFeatures.optimalTilingFeatures & requiredMipGenerationFeatures) != requiredMipGenerationFeatures) {
		Log::Warning("Vulkan::Device",
		             "Mipmap generation was requested for image, but is unavailable for format {}.",
		             vk::to_string(createInfo.Format));
		generateMips = false;
	}

	// Now we adjust a few flags to ensure the image is created with all of the proper usages and flags before creating
	// the image itself.
	ImageCreateInfo actualInfo = createInfo;
	ImageHandle handle;
	{
		// If we have initial data, we need to be able to transfer into the image.
		if (initialData) { actualInfo.Usage |= vk::ImageUsageFlagBits::eTransferDst; }

		// If we are generating mips, we need to be able to transfer from each mip level.
		if (generateMips) { actualInfo.Usage |= vk::ImageUsageFlagBits::eTransferSrc; }

		// If the image domain is transient, ensure the transient attachment usage is applied.
		if (createInfo.Domain == ImageDomain::Transient) {
			actualInfo.Usage |= vk::ImageUsageFlagBits::eTransientAttachment;
		}

		// If the number of mips was specified as 0, calculate the correct number of mips based on image size.
		if (createInfo.MipLevels == 0) { actualInfo.MipLevels = CalculateMipLevels(createInfo.Extent); }

		// The initial layout given to the ImageCreateInfo struct is what we will transition to after creation. The image
		// still must be created as undefined.
		actualInfo.InitialLayout = vk::ImageLayout::eUndefined;

		handle = ImageHandle(_imagePool.Allocate(*this, actualInfo));
	}

	// If applicable, create a default ImageView.
	{
		const bool hasView(actualInfo.Usage &
		                   (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment |
		                    vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled |
		                    vk::ImageUsageFlagBits::eStorage));
		if (hasView) {
			const ImageViewCreateInfo viewCI{.Image          = handle.Get(),
			                                 .Format         = createInfo.Format,
			                                 .Type           = GetImageViewType(createInfo),
			                                 .BaseMipLevel   = 0,
			                                 .MipLevels      = actualInfo.MipLevels,
			                                 .BaseArrayLayer = 0,
			                                 .ArrayLayers    = actualInfo.ArrayLayers};
			auto view = CreateImageView(viewCI);
			handle->SetDefaultView(view);
		}
	}

	// Here we copy over the initial image data (if present) and transition the image to the layout requested in
	// ImageCreateInfo.
	{
		CommandBufferHandle transitionCmd;
		// Now that we have the image, we copy over the initial data if we have some.
		if (initialData) {
			// Determine whether we can perform the transfer on a separate queue.
			bool separateTransfer = !_queues.SameQueue(QueueType::Graphics, QueueType::Transfer);
			separateTransfer      = false;  // TODO

			vk::AccessFlags finalTransitionSrcAccess;
			if (generateMips) {
				finalTransitionSrcAccess = vk::AccessFlagBits::eTransferRead;
			} else if (!separateTransfer) {
				finalTransitionSrcAccess = vk::AccessFlagBits::eTransferWrite;
			}

			const vk::AccessFlags prepareSrcAccess =
				separateTransfer ? vk::AccessFlags() : vk::AccessFlagBits::eTransferWrite;

			bool needMipmapBarrier  = true;
			bool needInitialBarrier = true;

			auto graphicsCmd = RequestCommandBuffer(CommandBufferType::Generic);

			CommandBufferHandle transferCmd;
			if (separateTransfer) {
				transferCmd = RequestCommandBuffer(CommandBufferType::AsyncTransfer);
			} else {
				transferCmd = graphicsCmd;
			}

			// Image Copy
			{
				transferCmd->ImageBarrier(*handle,
				                          vk::ImageLayout::eUndefined,
				                          vk::ImageLayout::eTransferDstOptimal,
				                          vk::PipelineStageFlagBits::eTopOfPipe,
				                          {},
				                          vk::PipelineStageFlagBits::eTransfer,
				                          vk::AccessFlagBits::eTransferWrite);
				transferCmd->CopyBufferToImage(*handle, *initialBuffer.Buffer, initialBuffer.ImageCopies);
			}

			// Submit Image Copy, with necessary ownership transfers
			if (separateTransfer) {
				vk::PipelineStageFlags dstStages =
					generateMips ? vk::PipelineStageFlagBits::eTransfer : handle->GetStageFlags();

				if (!_queues.SameFamily(QueueType::Graphics, QueueType::Transfer)) {
					needMipmapBarrier = false;
					if (generateMips) { needInitialBarrier = false; }

					const vk::ImageMemoryBarrier release(
						vk::AccessFlagBits::eTransferWrite,
						{},
						vk::ImageLayout::eTransferDstOptimal,
						generateMips ? vk::ImageLayout::eTransferSrcOptimal : createInfo.InitialLayout,
						_queues.Family(QueueType::Transfer),
						_queues.Family(QueueType::Graphics),
						handle->GetImage(),
						vk::ImageSubresourceRange(FormatToAspect(createInfo.Format),
					                            0,
					                            generateMips ? 1 : actualInfo.MipLevels,
					                            0,
					                            actualInfo.ArrayLayers));
					vk::ImageMemoryBarrier acquire = release;
					acquire.srcAccessMask          = {};
					acquire.dstAccessMask =
						generateMips ? vk::AccessFlagBits::eTransferRead
												 : (handle->GetAccessFlags() & ImageLayoutToPossibleAccess(createInfo.InitialLayout));

					transferCmd->Barrier(
						vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, nullptr, nullptr, release);
					graphicsCmd->Barrier(dstStages, dstStages, nullptr, nullptr, acquire);
				}

				std::vector<SemaphoreHandle> semaphores(1);
				Submit(transferCmd, nullptr, &semaphores);
				AddWaitSemaphore(CommandBufferType::Generic, semaphores[0], dstStages, true);
			}

			if (generateMips) {
				graphicsCmd->GenerateMipmaps(*handle,
				                             vk::ImageLayout::eTransferDstOptimal,
				                             vk::PipelineStageFlagBits::eTransfer,
				                             prepareSrcAccess,
				                             needMipmapBarrier);
			}

			if (needInitialBarrier) {
				graphicsCmd->ImageBarrier(
					*handle,
					generateMips ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eTransferDstOptimal,
					createInfo.InitialLayout,
					vk::PipelineStageFlagBits::eTransfer,
					finalTransitionSrcAccess,
					handle->GetStageFlags(),
					handle->GetAccessFlags() & ImageLayoutToPossibleAccess(createInfo.InitialLayout));
			}

			transitionCmd = std::move(graphicsCmd);
		} else if (createInfo.InitialLayout != vk::ImageLayout::eUndefined) {
			auto cmd = RequestCommandBuffer(CommandBufferType::Generic);

			cmd->ImageBarrier(*handle,
			                  actualInfo.InitialLayout,
			                  createInfo.InitialLayout,
			                  vk::PipelineStageFlagBits::eTopOfPipe,
			                  {},
			                  handle->GetStageFlags(),
			                  handle->GetAccessFlags() & ImageLayoutToPossibleAccess(createInfo.InitialLayout));
			transitionCmd = std::move(cmd);
		}

		if (transitionCmd) {
			LOCK();
			SubmitNoLock(transitionCmd, nullptr, nullptr);
		}
	}

	return handle;
}

ImageViewHandle Device::CreateImageView(const ImageViewCreateInfo& createInfo) {
	return ImageViewHandle(_imageViewPool.Allocate(*this, createInfo));
}

DescriptorSetAllocator* Device::RequestDescriptorSetAllocator(const DescriptorSetLayout& layout,
                                                              const uint32_t* stagesForBindings) {
	Utility::Hasher h;
	h.Data(sizeof(DescriptorSetLayout), &layout);
	h.Data(sizeof(uint32_t) * MaxDescriptorBindings, stagesForBindings);
	const auto hash = h.Get();

	auto* ret = _descriptorSetAllocators.Find(hash);
	if (!ret) { ret = _descriptorSetAllocators.EmplaceYield(hash, hash, *this, layout, stagesForBindings); }

	return ret;
}

FenceHandle Device::RequestFence() {
	LOCK();
	auto fence = AllocateFence();
	return FenceHandle(_fencePool.Allocate(*this, fence));
}

PipelineLayout* Device::RequestPipelineLayout(const ProgramResourceLayout& layout) {
	const auto hash = Utility::Hasher(layout).Get();

	auto* ret = _pipelineLayouts.Find(hash);
	if (!ret) { ret = _pipelineLayouts.EmplaceYield(hash, hash, *this, layout); }

	return ret;
}

Program* Device::RequestProgram(
	size_t vertCodeSize, const void* vertCode, size_t fragCodeSize, const void* fragCode, const std::string& debugName) {
	std::string vertName = "";
	std::string fragName = "";
	if (debugName.length() > 0) {
		vertName = fmt::format("{} (Vertex Shader)", debugName);
		fragName = fmt::format("{} (Fragment Shader)", debugName);
	}
	auto vert = RequestShader(vertCodeSize, vertCode, vertName);
	auto frag = RequestShader(fragCodeSize, fragCode, fragName);

	return RequestProgram(vert, frag, debugName);
}

Program* Device::RequestProgram(Shader* vertex, Shader* fragment, const std::string& debugName) {
	Utility::Hasher h;
	h(vertex->GetHash());
	h(fragment->GetHash());
	const auto hash = h.Get();

	Program* ret = _programs.Find(hash);
	if (!ret) {
		try {
			ret = _programs.EmplaceYield(hash, hash, *this, vertex, fragment, debugName);
		} catch (const std::exception& e) {
			Log::Error("Vulkan::Device", "Failed to create program: {}", e.what());
			return nullptr;
		}
	}

	return ret;
}

Program* Device::RequestProgram(const std::string& vertexGlsl,
                                const std::string& fragmentGlsl,
                                const std::string& debugName) {
	std::string vertName = "";
	std::string fragName = "";
	if (debugName.length() > 0) {
		vertName = fmt::format("{} (Vertex Shader)", debugName);
		fragName = fmt::format("{} (Fragment Shader)", debugName);
	}

	auto vert = RequestShader(vk::ShaderStageFlagBits::eVertex, vertexGlsl, vertName);
	auto frag = RequestShader(vk::ShaderStageFlagBits::eFragment, fragmentGlsl, fragName);
	if (vert && frag) {
		return RequestProgram(vert, frag, debugName);
	} else {
		return nullptr;
	}
}

SemaphoreHandle Device::RequestSemaphore(const std::string& debugName) {
	LOCK();
	auto semaphore = AllocateSemaphore();
	return SemaphoreHandle(_semaphorePool.Allocate(*this, semaphore, false, debugName));
}

Shader* Device::RequestShader(size_t codeSize, const void* code, const std::string& debugName) {
	Utility::Hasher h;
	h(codeSize);
	h.Data(codeSize, code);
	const auto hash = h.Get();

	Shader* shader = _shaders.Find(hash);
	if (!shader) {
		try {
			shader = _shaders.EmplaceYield(hash, hash, *this, codeSize, code);
		} catch (const std::exception& e) {
			Log::Error("Vulkan::Device", "Failed to create shader module: {}", e.what());
			return nullptr;
		}
	}

	if (debugName.length() > 0) { SetObjectName(shader->GetShaderModule(), debugName); }

	return shader;
}

Shader* Device::RequestShader(vk::ShaderStageFlagBits stage, const std::string& glsl, const std::string& debugName) {
	auto spirv = _shaderCompiler->Compile(stage, glsl);
	if (spirv.has_value()) {
		return RequestShader(spirv.value().size() * sizeof(uint32_t), spirv.value().data(), debugName);
	} else {
		return nullptr;
	}
}

ImageHandle Device::RequestTransientAttachment(const vk::Extent2D& extent,
                                               vk::Format format,
                                               uint32_t index,
                                               vk::SampleCountFlagBits samples,
                                               uint32_t layers) const {
	return _transientAttachmentAllocator->RequestAttachment(extent, format, index, samples, layers);
}

// Allocate a "cookie" to an object, which serves as a unique identifier for that object for the lifetime of the
// application.
uint64_t Device::AllocateCookie(Utility::Badge<Cookie>) {
#ifdef LUNA_VULKAN_MT
	return _nextCookie.fetch_add(16, std::memory_order_relaxed) + 16;
#else
	_nextCookie += 16;
	return _nextCookie;
#endif
}

SemaphoreHandle Device::ConsumeReleaseSemaphore(Utility::Badge<Swapchain>) {
	return std::move(_swapchainRelease);
}

void Device::DestroyBuffer(Utility::Badge<BufferDeleter>, Buffer* buffer) {
	MAYBE_LOCK(buffer);
	Frame().BuffersToDestroy.push_back(buffer);
}

void Device::DestroyImage(Utility::Badge<ImageDeleter>, Image* image) {
	MAYBE_LOCK(image);
	Frame().ImagesToDestroy.push_back(image);
}

void Device::DestroyImageView(Utility::Badge<ImageViewDeleter>, ImageView* view) {
	MAYBE_LOCK(view);
	Frame().ImageViewsToDestroy.push_back(view);
}

void Device::RecycleFence(Utility::Badge<FenceDeleter>, Fence* fence) {
	if (fence->GetFence()) {
		MAYBE_LOCK(fence);

		auto vkFence = fence->GetFence();

		if (fence->HasObservedWait()) {
			_device.resetFences(vkFence);
			ReleaseFence(vkFence);
		} else {
			Frame().FencesToRecycle.push_back(vkFence);
		}
	}

	_fencePool.Free(fence);
}

void Device::RecycleSemaphore(Utility::Badge<SemaphoreDeleter>, Semaphore* semaphore) {
	const auto vkSemaphore = semaphore->GetSemaphore();
	const auto value       = semaphore->GetTimelineValue();

	if (vkSemaphore && value == 0) {
		MAYBE_LOCK(semaphore);

		if (semaphore->IsSignalled()) {
			Frame().SemaphoresToDestroy.push_back(vkSemaphore);
		} else {
			Frame().SemaphoresToRecycle.push_back(vkSemaphore);
		}
	}

	_semaphorePool.Free(semaphore);
}

// Release a command buffer and return it to our pool.
void Device::ReleaseCommandBuffer(Utility::Badge<CommandBufferDeleter>, CommandBuffer* cmdBuf) {
	_commandBufferPool.Free(cmdBuf);
}

Framebuffer& Device::RequestFramebuffer(Utility::Badge<CommandBuffer>, const RenderPassInfo& info) {
	return _framebufferAllocator->RequestFramebuffer(info);
}

RenderPass& Device::RequestRenderPass(Utility::Badge<CommandBuffer>, const RenderPassInfo& info, bool compatible) {
	return RequestRenderPass(info, compatible);
}

RenderPass& Device::RequestRenderPass(Utility::Badge<FramebufferAllocator>,
                                      const RenderPassInfo& info,
                                      bool compatible) {
	return RequestRenderPass(info, compatible);
}

Sampler* Device::RequestSampler(const SamplerCreateInfo& createInfo) {
	Utility::Hasher h(createInfo);
	const auto hash = h.Get();
	auto* ret       = _samplers.Find(hash);
	if (!ret) { ret = _samplers.EmplaceYield(hash, hash, *this, createInfo); }

	return ret;
}

Sampler* Device::RequestSampler(StockSampler type) {
	return _stockSamplers[static_cast<int>(type)];
}

void Device::SetAcquireSemaphore(Utility::Badge<Swapchain>, uint32_t imageIndex, SemaphoreHandle& semaphore) {
	_swapchainAcquire         = std::move(semaphore);
	_swapchainAcquireConsumed = false;
	_swapchainIndex           = imageIndex;

	SetObjectName(_swapchainAcquire->GetSemaphore(), "Swapchain Acquire Semaphore");

	if (_swapchainAcquire) { _swapchainAcquire->_internalSync = true; }
}

void Device::SetupSwapchain(Utility::Badge<Swapchain>, Swapchain& swapchain) {
	WAIT_FOR_PENDING_COMMAND_BUFFERS();
	WaitIdleNoLock();

	const auto extent     = swapchain.GetExtent();
	const auto format     = swapchain.GetFormat();
	const auto& images    = swapchain.GetImages();
	const auto createInfo = ImageCreateInfo::RenderTarget(format, extent);
	_swapchainImages.clear();
	_swapchainImages.reserve(images.size());

	for (size_t i = 0; i < images.size(); ++i) {
		const auto& image = images[i];
		Image* img        = _imagePool.Allocate(*this, createInfo, image);
#ifdef LUNA_VULKAN_DEBUG
		const std::string imgName = fmt::format("Swapchain Image {}", i);
		SetObjectName(img->GetImage(), imgName);
#endif
		ImageHandle handle(img);
		handle->_internalSync = true;
		handle->SetSwapchainLayout(vk::ImageLayout::ePresentSrcKHR);

		const ImageViewCreateInfo viewCI{.Image = img, .Format = format, .Type = vk::ImageViewType::e2D};
		ImageViewHandle view(_imageViewPool.Allocate(*this, viewCI));
#ifdef LUNA_VULKAN_DEBUG
		const std::string viewName = fmt::format("Swapchain Image View {}", i);
		SetObjectName(view->GetImageView(), viewName);
#endif
		handle->SetDefaultView(view);
		view->_internalSync = true;

		_swapchainImages.push_back(handle);
	}
}

void Device::AddWaitSemaphoreNoLock(QueueType queueType,
                                    SemaphoreHandle semaphore,
                                    vk::PipelineStageFlags stages,
                                    bool flush) {
	if (flush) { FlushFrameNoLock(queueType); }

	auto& queueData = _queueData[static_cast<int>(queueType)];

	semaphore->SignalPendingWait();
	queueData.WaitSemaphores.push_back(semaphore);
	queueData.WaitStages.push_back(stages);
	queueData.NeedsFence = true;
}

void Device::EndFrameNoLock() {
	constexpr static const QueueType flushOrder[] = {QueueType::Transfer, QueueType::Graphics, QueueType::Compute};
	InternalFence submitFence;

	for (auto& type : flushOrder) {
		auto& queueData = _queueData[static_cast<int>(type)];
		if (queueData.NeedsFence || !Frame().Submissions[static_cast<int>(type)].empty()) {
			SubmitQueue(type, &submitFence, nullptr);
			if (submitFence.Fence) {
				Frame().FencesToAwait.push_back(submitFence.Fence);
				Frame().FencesToRecycle.push_back(submitFence.Fence);
			}
			queueData.NeedsFence = false;
		}
	}
}

void Device::FlushFrameNoLock(QueueType queueType) {
	if (_queues.Queue(queueType)) { SubmitQueue(queueType, nullptr, nullptr); }
}

void Device::SubmitNoLock(CommandBufferHandle cmd, FenceHandle* fence, std::vector<SemaphoreHandle>* semaphores) {
	const auto queueType = GetQueueType(cmd->GetType());
	auto& submissions    = Frame().Submissions[static_cast<int>(queueType)];

	cmd->End();

	submissions.push_back(std::move(cmd));

	InternalFence submitFence;
	if (fence || semaphores) {
		SubmitQueue(queueType, fence ? &submitFence : nullptr, semaphores);

		if (fence) {
			if (submitFence.TimelineValue != 0) {
				*fence = FenceHandle(_fencePool.Allocate(*this, submitFence.TimelineSemaphore, submitFence.TimelineValue));
			} else {
				*fence = FenceHandle(_fencePool.Allocate(*this, submitFence.Fence));
			}
		}
	}

	--_pendingCommandBuffers;
#ifdef LUNA_VULKAN_MT
	_pendingCommandBuffersCondition.notify_all();
#endif
}

void Device::SubmitQueue(QueueType queueType, InternalFence* submitFence, std::vector<SemaphoreHandle>* semaphores) {
	auto& queueData          = _queueData[static_cast<int>(queueType)];
	auto& submissions        = Frame().Submissions[static_cast<int>(queueType)];
	const bool hasSemaphores = semaphores != nullptr && semaphores->size() != 0;

	if (submissions.empty() && submitFence == nullptr && !hasSemaphores) { return; }

	if (queueType != QueueType::Transfer) { FlushFrameNoLock(QueueType::Transfer); }

	vk::Queue queue                                     = _queues.Queue(queueType);
	vk::Semaphore timelineSemaphore                     = queueData.TimelineSemaphore;
	uint64_t timelineValue                              = ++queueData.TimelineValue;
	Frame().TimelineValues[static_cast<int>(queueType)] = timelineValue;

	// Batch all of our command buffers into as few submissions as possible. Increment batch whenever we need to use a
	// signal semaphore.
	constexpr static const int MaxSubmissions = 8;
	struct SubmitBatch {
		bool HasTimeline = false;
		std::vector<vk::CommandBuffer> CommandBuffers;
		std::vector<vk::Semaphore> SignalSemaphores;
		std::vector<uint64_t> SignalValues;
		std::vector<vk::Semaphore> WaitSemaphores;
		std::vector<vk::PipelineStageFlags> WaitStages;
		std::vector<uint64_t> WaitValues;
	};
	std::array<SubmitBatch, MaxSubmissions> batches;
	uint8_t batch = 0;

	// First, add all of the wait semaphores we've accumulated over the frame to the first batch. These usually come from
	// inter-queue staging buffers.
	for (size_t i = 0; i < queueData.WaitSemaphores.size(); ++i) {
		auto& semaphoreHandle = queueData.WaitSemaphores[i];
		auto semaphore        = semaphoreHandle->Consume();
		auto waitStages       = queueData.WaitStages[i];
		auto waitValue        = semaphoreHandle->GetTimelineValue();

		batches[batch].WaitSemaphores.push_back(semaphore);
		batches[batch].WaitStages.push_back(waitStages);
		batches[batch].WaitValues.push_back(waitValue);
		batches[batch].HasTimeline = batches[batch].HasTimeline || waitValue != 0;
	}
	queueData.WaitSemaphores.clear();
	queueData.WaitStages.clear();

	// Add our command buffers.
	for (auto& cmdBufHandle : submissions) {
		const vk::PipelineStageFlags swapchainStages = cmdBufHandle->GetSwapchainStages();

		if (swapchainStages && !_swapchainAcquireConsumed) {
			if (_swapchainAcquire && _swapchainAcquire->GetSemaphore()) {
				if (!batches[batch].CommandBuffers.empty() || !batches[batch].SignalSemaphores.empty()) { ++batch; }

				const auto value = _swapchainAcquire->GetTimelineValue();
				batches[batch].WaitSemaphores.push_back(_swapchainAcquire->GetSemaphore());
				batches[batch].WaitStages.push_back(swapchainStages);
				batches[batch].WaitValues.push_back(value);

				if (!value) { Frame().SemaphoresToRecycle.push_back(_swapchainAcquire->GetSemaphore()); }

				_swapchainAcquire->Consume();
				_swapchainAcquireConsumed = true;
				_swapchainAcquire.Reset();
			}

			if (!batches[batch].SignalSemaphores.empty()) {
				++batch;
				assert(batch < MaxSubmissions);
			}

			batches[batch].CommandBuffers.push_back(cmdBufHandle->GetCommandBuffer());

			vk::Semaphore release            = AllocateSemaphore();
			_swapchainRelease                = SemaphoreHandle(_semaphorePool.Allocate(*this, release, true));
			_swapchainRelease->_internalSync = true;
			SetObjectName(release, "Swapchain Release Semaphore");
			batches[batch].SignalSemaphores.push_back(release);
			batches[batch].SignalValues.push_back(0);
		} else {
			if (!batches[batch].SignalSemaphores.empty()) {
				++batch;
				assert(batch < MaxSubmissions);
			}

			batches[batch].CommandBuffers.push_back(cmdBufHandle->GetCommandBuffer());
		}
	}
	submissions.clear();

	// Only use a fence if we have to. Prefer using the timeline semaphore for each queue.
	vk::Fence fence = VK_NULL_HANDLE;
	if (submitFence && !_gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore) {
		fence              = AllocateFence();
		submitFence->Fence = fence;
	}

	// Emit any necessary semaphores from the final batch.
	if (_gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore) {
		batches[batch].SignalSemaphores.push_back(timelineSemaphore);
		batches[batch].SignalValues.push_back(timelineValue);
		batches[batch].HasTimeline = true;

		if (submitFence) {
			submitFence->Fence             = VK_NULL_HANDLE;
			submitFence->TimelineSemaphore = timelineSemaphore;
			submitFence->TimelineValue     = timelineValue;
		}

		if (hasSemaphores) {
			for (size_t i = 0; i < semaphores->size(); ++i) {
				(*semaphores)[i] = SemaphoreHandle(_semaphorePool.Allocate(*this, timelineSemaphore, timelineValue));
			}
		}
	} else {
		if (submitFence) {
			submitFence->TimelineSemaphore = VK_NULL_HANDLE;
			submitFence->TimelineValue     = 0;
		}

		if (hasSemaphores) {
			for (size_t i = 0; i < semaphores->size(); ++i) {
				vk::Semaphore sem = AllocateSemaphore();
				batches[batch].SignalSemaphores.push_back(sem);
				batches[batch].SignalValues.push_back(0);
				(*semaphores)[i] = SemaphoreHandle(_semaphorePool.Allocate(*this, sem, true));
			}
		}
	}

	// Build our submit info structures.
	std::array<vk::SubmitInfo, MaxSubmissions> submits;
	std::array<vk::TimelineSemaphoreSubmitInfo, MaxSubmissions> timelineSubmits;
	for (uint8_t i = 0; i <= batch; ++i) {
		submits[i] = vk::SubmitInfo(
			batches[i].WaitSemaphores, batches[i].WaitStages, batches[i].CommandBuffers, batches[i].SignalSemaphores);
		if (batches[i].HasTimeline) {
			timelineSubmits[i] = vk::TimelineSemaphoreSubmitInfo(batches[i].WaitValues, batches[i].SignalValues);
			submits[i].pNext   = &timelineSubmits[i];
		}
	}

	// Compact our submissions to remove any empty ones.
	uint32_t submitCount = 0;
	for (size_t i = 0; i < submits.size(); ++i) {
		if (submits[i].waitSemaphoreCount || submits[i].commandBufferCount || submits[i].signalSemaphoreCount) {
			if (i != submitCount) { submits[submitCount] = submits[i]; }
			++submitCount;
		}
	}

	// Finally, submit it all!
	const auto submitResult = queue.submit(submitCount, submits.data(), fence);
	if (submitResult != vk::Result::eSuccess) {
		Log::Error("Vulkan::Device", "Error occurred when submitting command buffers: {}", vk::to_string(submitResult));
	}

	// If we weren't able to use a timeline semaphore, we need to make sure there is a fence in
	// place to wait for completion.
	if (!_gpuInfo.AvailableFeatures.TimelineSemaphore.timelineSemaphore) { queueData.NeedsFence = true; }
}

void Device::SubmitStaging(CommandBufferHandle& cmd, vk::BufferUsageFlags usage, bool flush) {
	const auto access   = BufferUsageToAccess(usage);
	const auto stages   = BufferUsageToStages(usage);
	const auto srcQueue = _queues.Queue(GetQueueType(cmd->GetType()));

	if (_queues.SameQueue(QueueType::Graphics, QueueType::Compute)) {
		cmd->Barrier(vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, stages, access);
		SubmitNoLock(cmd, nullptr, nullptr);
	} else {
		const auto computeStages =
			stages & (vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eDrawIndirect);
		const auto computeAccess = access & (vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eShaderRead |
		                                     vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eTransferRead |
		                                     vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eUniformRead);

		const auto graphicsStages = stages & vk::PipelineStageFlagBits::eAllGraphics;

		if (srcQueue == _queues.Queue(QueueType::Graphics)) {
			cmd->Barrier(vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, graphicsStages, access);

			if (computeStages) {
				std::vector<SemaphoreHandle> semaphores(1);
				SubmitNoLock(cmd, nullptr, &semaphores);
				AddWaitSemaphoreNoLock(QueueType::Compute, semaphores[0], computeStages, flush);
			} else {
				SubmitNoLock(cmd, nullptr, nullptr);
			}
		} else if (srcQueue == _queues.Queue(QueueType::Compute)) {
			cmd->Barrier(
				vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, computeStages, computeAccess);

			if (graphicsStages) {
				std::vector<SemaphoreHandle> semaphores(1);
				SubmitNoLock(cmd, nullptr, &semaphores);
				AddWaitSemaphoreNoLock(QueueType::Graphics, semaphores[0], graphicsStages, flush);
			} else {
				SubmitNoLock(cmd, nullptr, nullptr);
			}
		} else {
			if (graphicsStages && computeStages) {
				std::vector<SemaphoreHandle> semaphores(2);
				SubmitNoLock(cmd, nullptr, &semaphores);
				AddWaitSemaphoreNoLock(QueueType::Graphics, semaphores[0], graphicsStages, flush);
				AddWaitSemaphoreNoLock(QueueType::Compute, semaphores[1], computeStages, flush);
			} else if (graphicsStages) {
				std::vector<SemaphoreHandle> semaphores(1);
				SubmitNoLock(cmd, nullptr, &semaphores);
				AddWaitSemaphoreNoLock(QueueType::Graphics, semaphores[0], graphicsStages, flush);
			} else if (computeStages) {
				std::vector<SemaphoreHandle> semaphores(1);
				SubmitNoLock(cmd, nullptr, &semaphores);
				AddWaitSemaphoreNoLock(QueueType::Compute, semaphores[0], computeStages, flush);
			} else {
				SubmitNoLock(cmd, nullptr, nullptr);
			}
		}
	}
}

void Device::WaitIdleNoLock() {
	if (!_frameContexts.empty()) {}

	if (_device) { _device.waitIdle(); }

	if (_framebufferAllocator) { _framebufferAllocator->Clear(); }
	if (_transientAttachmentAllocator) { _transientAttachmentAllocator->Clear(); }

	for (auto& context : _frameContexts) { context->Begin(); }
}

vk::Fence Device::AllocateFence() {
	if (_availableFences.empty()) {
		Log::Trace("Vulkan::Device", "Creating new Fence.");

		const vk::FenceCreateInfo fenceCI;
		vk::Fence fence = _device.createFence(fenceCI);

		return fence;
	}

	vk::Fence fence = _availableFences.back();
	_availableFences.pop_back();

	return fence;
}

vk::Semaphore Device::AllocateSemaphore() {
	if (_availableSemaphores.empty()) {
		Log::Trace("Vulkan::Device", "Creating new Semaphore.");

		const vk::SemaphoreCreateInfo semaphoreCI;
		vk::Semaphore semaphore = _device.createSemaphore(semaphoreCI);

		return semaphore;
	}

	vk::Semaphore semaphore = _availableSemaphores.back();
	_availableSemaphores.pop_back();

	return semaphore;
}

void Device::ReleaseFence(vk::Fence fence) {
	_availableFences.push_back(fence);
}

void Device::ReleaseSemaphore(vk::Semaphore semaphore) {
	SetObjectName(semaphore, "");
	_availableSemaphores.push_back(semaphore);
}

RenderPass& Device::RequestRenderPass(const RenderPassInfo& info, bool compatible) {
	const auto hash = HashRenderPassInfo(info, compatible);
	auto* ret       = _renderPasses.Find(hash);
	if (!ret) { ret = _renderPasses.EmplaceYield(hash, hash, *this, info); }

	return *ret;
}

// Return our current frame context.
Device::FrameContext& Device::Frame() {
	return *_frameContexts[_currentFrameContext];
}

#ifdef LUNA_VULKAN_DEBUG
// Set an object's debug name for validation layers and RenderDoc functionality.
void Device::SetObjectNameImpl(vk::ObjectType type, uint64_t handle, const std::string& name) {
	if (!_extensions.DebugUtils) { return; }

	const vk::DebugUtilsObjectNameInfoEXT nameInfo(type, handle, name.c_str());
	_device.setDebugUtilsObjectNameEXT(nameInfo);
}
#endif

Device::FrameContext::FrameContext(Device& device, uint32_t frameIndex) : Parent(device), FrameIndex(frameIndex) {
#ifdef LUNA_VULKAN_MT
#	error Multithread not implemented yet
#else
	const uint32_t threadCount = 1;
#endif

	for (uint32_t type = 0; type < QueueTypeCount; ++type) {
		TimelineValues[type] = 0;
		const auto family    = Parent._queues.Families[type];
		for (uint32_t thread = 0; thread < threadCount; ++thread) {
			CommandPools[type].emplace_back(std::make_unique<CommandPool>(Parent, family));
			Parent.SetObjectName(CommandPools[type].back()->GetCommandPool(),
			                     fmt::format("{} Command Pool {}", VulkanEnumToString(static_cast<QueueType>(type)), thread));
		}
	}
}

Device::FrameContext::~FrameContext() noexcept {}

void Device::FrameContext::Begin() {
	const auto& device = Parent.GetDevice();

	// Wait on our timeline semaphores to ensure this frame context has completed all of its pending work.
	{
		bool hasTimelineSemaphores = true;
		for (auto& queue : Parent._queueData) {
			if (!queue.TimelineSemaphore) {
				hasTimelineSemaphores = false;
				break;
			}
		}
		if (hasTimelineSemaphores) {
			uint32_t semaphoreCount = 0;
			std::array<vk::Semaphore, QueueTypeCount> semaphores;
			std::array<uint64_t, QueueTypeCount> values;
			for (size_t i = 0; i < QueueTypeCount; ++i) {
				if (TimelineValues[i]) {
					semaphores[semaphoreCount] = Parent._queueData[i].TimelineSemaphore;
					values[semaphoreCount]     = TimelineValues[i];
					++semaphoreCount;
				}
			}

			if (semaphoreCount) {
				const vk::SemaphoreWaitInfo waitInfo({}, semaphoreCount, semaphores.data(), values.data());
				const auto waitResult = device.waitSemaphoresKHR(waitInfo, std::numeric_limits<uint64_t>::max());
				if (waitResult != vk::Result::eSuccess) {
					Log::Error("Vulkan::Device", "Failed to wait on timeline semaphores!");
				}
			}
		}
	}

	// Wait on our fences to ensure this frame context has completed all of its pending work.
	// If we are able to use timeline semaphores, this should never be needed.
	if (!FencesToAwait.empty()) {
		const auto waitResult = device.waitForFences(FencesToAwait, VK_TRUE, std::numeric_limits<uint64_t>::max());
		if (waitResult != vk::Result::eSuccess) { Log::Error("Vulkan::Device", "Failed to wait on submit fences!"); }
		FencesToAwait.clear();
	}

	// Reset all of the fences that we used this frame.
	if (!FencesToRecycle.empty()) {
		device.resetFences(FencesToRecycle);
		for (auto& fence : FencesToRecycle) { Parent.ReleaseFence(fence); }
		FencesToRecycle.clear();
	}

	// Reset all of our command pools.
	{
		for (auto& queuePools : CommandPools) {
			for (auto& pool : queuePools) { pool->Reset(); }
		}
	}

	// Destroy or recycle all of our other resources that are no longer in use.
	{
		for (auto& buffer : BuffersToDestroy) { Parent._bufferPool.Free(buffer); }
		for (auto& image : ImagesToDestroy) { Parent._imagePool.Free(image); }
		for (auto& view : ImageViewsToDestroy) { Parent._imageViewPool.Free(view); }
		for (auto& semaphore : SemaphoresToDestroy) { device.destroySemaphore(semaphore); }
		for (auto& semaphore : SemaphoresToRecycle) { Parent.ReleaseSemaphore(semaphore); }
		BuffersToDestroy.clear();
		ImagesToDestroy.clear();
		ImageViewsToDestroy.clear();
		SemaphoresToDestroy.clear();
		SemaphoresToRecycle.clear();
	}
}
}  // namespace Vulkan
}  // namespace Luna
