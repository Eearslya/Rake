#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <Luna/Graphics/Graphics.hpp>
#include <Luna/Graphics/Vulkan/Common.hpp>
#include <Luna/Platform/Input.hpp>
#include <Luna/Utility/Module.hpp>

namespace Luna {
namespace UI {
struct Theme;
}

struct ImGuiRenderData;
struct ImGuiWindowData;

class UIManager : public Utility::Module::Registrar<UIManager> {
	static inline const bool Registered = Register("UIManager", Stage::Post, Depends<Graphics, Input>());

 public:
	UIManager();
	~UIManager() noexcept;

	void Update() override;

	void BeginFrame();
	void EndFrame();
	void Render(Vulkan::CommandBufferHandle& cmd);
	void SetDockspace(bool dockspace);
	void SetTheme(const UI::Theme& theme);

	void RenderWindowOuterBorders(ImGuiWindow* window) const;

 private:
	void SetRenderState(Vulkan::CommandBufferHandle& cmd, ImDrawData* drawData) const;

	Vulkan::Device& _device;
	std::unique_ptr<ImGuiRenderData> _renderData;
	std::unique_ptr<ImGuiWindowData> _windowData;
	Vulkan::ImageHandle _fontTexture;
	Vulkan::Program* _program     = nullptr;
	Vulkan::Sampler* _fontSampler = nullptr;
	Vulkan::BufferHandle _vertexBuffer;
	Vulkan::BufferHandle _indexBuffer;
	bool _dockspace = false;
};
}  // namespace Luna
