#pragma once

#include <Luna/Graphics/Vulkan/Common.hpp>
#include <Luna/Graphics/Vulkan/DescriptorSet.hpp>

namespace Luna {
namespace Vulkan {
struct ShaderResourceLayout {
	DescriptorSetLayout SetLayouts[MaxDescriptorSets] = {};
	uint32_t BindlessSetMask                          = 0;
	uint32_t InputMask                                = 0;
	uint32_t OutputMask                               = 0;
	uint32_t SpecConstantMask                         = 0;
	uint32_t PushConstantSize                         = 0;
};

struct ProgramResourceLayout {
	DescriptorSetLayout SetLayouts[MaxDescriptorSets]                    = {};
	uint32_t AttributeMask                                               = 0;
	uint32_t BindlessDescriptorSetMask                                   = 0;
	uint32_t CombinedSpecConstantMask                                    = 0;
	uint32_t DescriptorSetMask                                           = 0;
	uint32_t RenderTargetMask                                            = 0;
	uint32_t SpecConstantMask[ShaderStageCount]                          = {};
	uint32_t StagesForBindings[MaxDescriptorSets][MaxDescriptorBindings] = {};
	uint32_t StagesForSets[MaxDescriptorSets]                            = {};
	vk::PushConstantRange PushConstantRange                              = {};
	Utility::Hash PushConstantLayoutHash                                 = {};
};

class PipelineLayout : public HashedObject<PipelineLayout>, Utility::NonCopyable {
 public:
	PipelineLayout(Utility::Hash hash, Device& device, const ProgramResourceLayout& resourceLayout);
	~PipelineLayout() noexcept;

	DescriptorSetAllocator* GetAllocator(uint32_t set) const {
		return _setAllocators[set];
	}
	vk::PipelineLayout GetPipelineLayout() const {
		return _pipelineLayout;
	}
	const ProgramResourceLayout& GetResourceLayout() const {
		return _resourceLayout;
	}

 private:
	Device& _device;
	vk::PipelineLayout _pipelineLayout;
	ProgramResourceLayout _resourceLayout;
	std::array<DescriptorSetAllocator*, MaxDescriptorSets> _setAllocators;
};

class Shader : public HashedObject<Shader>, Utility::NonCopyable {
 public:
	Shader(Utility::Hash hash, Device& device, size_t codeSize, const void* code);
	~Shader() noexcept;

	const ShaderResourceLayout& GetResourceLayout() const {
		return _layout;
	}
	vk::ShaderModule GetShaderModule() const {
		return _shaderModule;
	}

 private:
	Device& _device;
	vk::ShaderModule _shaderModule;
	ShaderResourceLayout _layout;
};

class Program : public HashedObject<Program>, Utility::NonCopyable {
 public:
	Program(Utility::Hash hash, Device& device, Shader* vertex, Shader* fragment, const std::string& Name = "");
	Program(Utility::Hash hash, Device& device, Shader* compute);
	~Program() noexcept;

	const std::string& GetName() const {
		return _name;
	}
	PipelineLayout* GetPipelineLayout() const {
		return _pipelineLayout;
	}
	const ProgramResourceLayout& GetResourceLayout() const {
		return _layout;
	}
	Shader* GetShader(ShaderStage stage) const {
		return _shaders[static_cast<int>(stage)];
	}

	vk::Pipeline GetPipeline(Utility::Hash hash) const;
	vk::Pipeline AddPipeline(Utility::Hash hash, vk::Pipeline pipeline) const;

 private:
	void Bake();

	Device& _device;
	ProgramResourceLayout _layout;
	std::array<Shader*, ShaderStageCount> _shaders = {};
	PipelineLayout* _pipelineLayout                = nullptr;
	mutable VulkanCache<Utility::IntrusivePODWrapper<vk::Pipeline>> _pipelines;
	std::string _name;
};
}  // namespace Vulkan
}  // namespace Luna

template <>
struct std::hash<Luna::Vulkan::ProgramResourceLayout> {
	size_t operator()(const Luna::Vulkan::ProgramResourceLayout& layout) {
		Luna::Utility::Hasher h;

		h.Data(sizeof(layout.SetLayouts), layout.SetLayouts);
		h.Data(sizeof(layout.StagesForBindings), layout.StagesForBindings);
		h.Data(sizeof(layout.SpecConstantMask), layout.SpecConstantMask);
		h(layout.PushConstantRange.stageFlags);
		h(layout.PushConstantRange.size);
		h(layout.AttributeMask);
		h(layout.RenderTargetMask);

		return static_cast<size_t>(h.Get());
	}
};
