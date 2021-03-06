#include <vk_mem_alloc.h>

#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/Image.hpp>
#include <Luna/Utility/Log.hpp>

namespace Luna {
namespace Vulkan {
vk::ImageViewType GetImageViewType(const ImageCreateInfo& createInfo) {
	switch (createInfo.Type) {
		case vk::ImageType::e1D:
			return createInfo.ArrayLayers > 1 ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
		case vk::ImageType::e2D:
			if ((createInfo.Flags & ImageCreateFlagBits::CreateCubeCompatible) && (createInfo.ArrayLayers % 6) == 0) {
				return createInfo.ArrayLayers > 6 ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
			}
			return createInfo.ArrayLayers > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
		case vk::ImageType::e3D:
			return vk::ImageViewType::e3D;
	}

	assert(false && "Invalid ImageCreateInfo!");
	return vk::ImageViewType::e2D;
}

void ImageDeleter::operator()(Image* image) {
	image->_device.DestroyImage({}, image);
	image->_defaultView->SetInternalSync();
}

void ImageViewDeleter::operator()(ImageView* view) {
	view->_device.DestroyImageView({}, view);
}

Image::Image(Device& device, const ImageCreateInfo& createInfo)
		: Cookie(device), _device(device), _createInfo(createInfo) {
	const vk::ImageCreateInfo imageCI(createInfo.Flags & ImageCreateFlagBits::CreateCubeCompatible
	                                    ? vk::ImageCreateFlagBits::eCubeCompatible
	                                    : vk::ImageCreateFlags{},
	                                  createInfo.Type,
	                                  createInfo.Format,
	                                  createInfo.Extent,
	                                  createInfo.MipLevels,
	                                  createInfo.ArrayLayers,
	                                  createInfo.Samples,
	                                  vk::ImageTiling::eOptimal,
	                                  createInfo.Usage,
	                                  vk::SharingMode::eExclusive,
	                                  nullptr,
	                                  vk::ImageLayout::eUndefined);
	VmaAllocationCreateInfo imageAI{.usage = VMA_MEMORY_USAGE_AUTO};
	if (createInfo.Usage & vk::ImageUsageFlagBits::eTransientAttachment) {
		imageAI.preferredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	}

	VkImage image;
	VmaAllocationInfo allocationInfo;
	const vk::Result createResult =
		static_cast<vk::Result>(vmaCreateImage(_device.GetAllocator(),
	                                         reinterpret_cast<const VkImageCreateInfo*>(&imageCI),
	                                         &imageAI,
	                                         &image,
	                                         &_allocation,
	                                         &allocationInfo));
	if (createResult != vk::Result::eSuccess) {
		throw std::runtime_error(fmt::format("Vulkan::Image", "Error creating image: {}", vk::to_string(createResult)));
	}
	Log::Trace("Vulkan::Image", "Image created.");

	_image       = image;
	_accessFlags = ImageUsageToAccess(imageCI.usage);
	_stageFlags  = ImageUsageToStages(imageCI.usage);
	_imageSize   = allocationInfo.size;
}

Image::Image(Device& device, const ImageCreateInfo& createInfo, vk::Image image)
		: Cookie(device), _device(device), _image(image), _createInfo(createInfo), _ownsImage(false) {}

Image::~Image() noexcept {
	auto dev = _device.GetDevice();

	if (_ownsImage) {
		if (_image) { dev.destroyImage(_image); }
		if (_allocation) { vmaFreeMemory(_device.GetAllocator(), _allocation); }
	}
}

vk::ImageLayout Image::GetLayout(vk::ImageLayout optimal) const {
	return _layoutType == ImageLayoutType::Optimal ? optimal : vk::ImageLayout::eGeneral;
}

ImageView::ImageView(Device& device, const ImageViewCreateInfo& createInfo)
		: Cookie(device), _device(device), _createInfo(createInfo) {
	const auto& imageCI = _createInfo.Image->GetCreateInfo();

	vk::ImageViewCreateInfo viewCI({},
	                               _createInfo.Image->GetImage(),
	                               _createInfo.Type,
	                               _createInfo.Format,
	                               vk::ComponentMapping(),
	                               vk::ImageSubresourceRange(FormatToAspect(_createInfo.Format),
	                                                         _createInfo.BaseMipLevel,
	                                                         _createInfo.MipLevels,
	                                                         _createInfo.BaseArrayLayer,
	                                                         _createInfo.ArrayLayers));

	// Create our main, "default" view.
	_imageView = _device.GetDevice().createImageView(viewCI);
	Log::Trace("Vulkan::ImageView", "Image View created.");

	// If applicable, create depth and stencil views.
	{
		if (_createInfo.Type != vk::ImageViewType::eCube && _createInfo.Type != vk::ImageViewType::eCubeArray &&
		    _createInfo.Type != vk::ImageViewType::e3D &&
		    viewCI.subresourceRange.aspectMask == (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil) &&
		    imageCI.Usage & ~vk::ImageUsageFlagBits::eDepthStencilAttachment) {
			Log::Trace("Vulkan::ImageView", "- Creating Depth/Stencil views.");
			auto dsView = viewCI;

			dsView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			_depthView                         = _device.GetDevice().createImageView(dsView);

			dsView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eStencil;
			_stencilView                       = _device.GetDevice().createImageView(dsView);
		}
	}

	// If applicable, create render target views.
	{
		if (viewCI.viewType != vk::ImageViewType::e3D &&
		    (imageCI.Usage &
		     (vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment)) &&
		    ((viewCI.subresourceRange.levelCount > 1 && viewCI.subresourceRange.levelCount != VK_REMAINING_MIP_LEVELS) ||
		     (viewCI.subresourceRange.layerCount > 1 && viewCI.subresourceRange.layerCount != VK_REMAINING_ARRAY_LAYERS))) {
			Log::Trace("Vulkan::ImageView", "- Creating Render Target views.");
			_renderTargetViews.reserve(viewCI.subresourceRange.layerCount);

			auto rtView                        = viewCI;
			rtView.viewType                    = vk::ImageViewType::e2D;
			rtView.subresourceRange.levelCount = 1;
			rtView.subresourceRange.layerCount = 1;
			for (uint32_t layer = 0; layer < viewCI.subresourceRange.layerCount; ++layer) {
				rtView.subresourceRange.baseArrayLayer = layer + viewCI.subresourceRange.baseArrayLayer;
				_renderTargetViews.push_back(_device.GetDevice().createImageView(rtView));
			}
		}
	}
}

ImageView::~ImageView() noexcept {
	if (_imageView) { _device.GetDevice().destroyImageView(_imageView); }
	if (_depthView) { _device.GetDevice().destroyImageView(_depthView); }
	if (_stencilView) { _device.GetDevice().destroyImageView(_stencilView); }
	for (auto& view : _renderTargetViews) { _device.GetDevice().destroyImageView(view); }
}

vk::ImageView ImageView::GetRenderTargetView(uint32_t layer) const {
	if (_createInfo.Image->GetCreateInfo().Domain == ImageDomain::Transient) { return _imageView; }

	if (_renderTargetViews.empty()) {
		return _imageView;
	} else {
		return _renderTargetViews[layer];
	}
}
}  // namespace Vulkan
}  // namespace Luna
