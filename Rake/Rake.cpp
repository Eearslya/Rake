#include "Rake.hpp"

#include <Luna.hpp>
#include <Luna/Graphics/Vulkan/CommandBuffer.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>

using namespace Luna;

Rake::Rake() : App("Rake") {}

void Rake::Start() {
	Window::Get()->OnClosed += []() { Engine::Get()->Shutdown(); };
	Window::Get()->Maximize();
	Window::Get()->SetTitle("Rake");

	Graphics::Get()->OnRender += [this]() { Render(); };
}

void Rake::Update() {}

void Rake::Stop() {}

void Rake::BeginDockspace() const {
	auto ui           = UIManager::Get();
	ImGuiStyle& style = ImGui::GetStyle();

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::Begin("Luna Rake Dockspace",
	             nullptr,
	             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
	               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
	               ImGuiWindowFlags_NoNavFocus);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(370.0f, style.WindowMinSize.y));
	const ImGuiID dockspaceID = ImGui::GetID("LunaRakeDockspace");
	ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
	ImGui::PopStyleVar();
}

void Rake::Render() {
	auto& device = Graphics::Get()->GetDevice();

	auto cmdBuf = device.RequestCommandBuffer(Vulkan::CommandBufferType::Generic, "Main Command Buffer");

	UIManager::Get()->BeginFrame();
	RenderRakeUI();
	UIManager::Get()->Render(cmdBuf);
	UIManager::Get()->EndFrame();

	device.Submit(cmdBuf);
}

void Rake::RenderRakeUI() {
	BeginDockspace();
	RenderViewport();
}

void Rake::RenderViewport() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(_viewportSize.x, _viewportSize.y), ImGuiCond_FirstUseEver);
	const bool draw = ImGui::Begin("Render Result##Viewport");
	ImGui::PopStyleVar();
	if (draw) {
		const auto viewportSize = ImGui::GetContentRegionAvail();
		_viewportSize           = glm::uvec2(viewportSize.x, viewportSize.y);
	}
	ImGui::End();
}
