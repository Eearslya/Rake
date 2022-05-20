#include <GLFW/glfw3.h>

#include <Luna/Graphics/Graphics.hpp>
#include <Luna/Graphics/Vulkan/Buffer.hpp>
#include <Luna/Graphics/Vulkan/CommandBuffer.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/Format.hpp>
#include <Luna/Graphics/Vulkan/Image.hpp>
#include <Luna/Graphics/Vulkan/RenderPass.hpp>
#include <Luna/Graphics/Vulkan/Sampler.hpp>
#include <Luna/Graphics/Vulkan/Shader.hpp>
#include <Luna/Platform/Input.hpp>
#include <Luna/Platform/Window.hpp>
#include <Luna/UI/Theme.hpp>
#include <Luna/UI/UIManager.hpp>
#include <Luna/Utility/Log.hpp>

namespace Luna {
struct ImGuiRenderData {
	Vulkan::Device* Device = nullptr;
};

struct ImGuiWindowData {
	GLFWwindow* Window                               = nullptr;
	double Time                                      = 0.0;
	GLFWwindow* MouseWindow                          = nullptr;
	bool MouseJustPressed[ImGuiMouseButton_COUNT]    = {false};
	GLFWcursor* MouseCursors[ImGuiMouseCursor_COUNT] = {nullptr};
	GLFWwindow* KeyOwnerWindows[512]                 = {nullptr};
};

struct PushConstant {
	float ScaleX;
	float ScaleY;
	float TranslateX;
	float TranslateY;
	float ColorCorrect;
};

UIManager::UIManager() : _device(Graphics::Get()->GetDevice()) {
	Log::Trace("UIManager", "Initializing ImGui interface.");

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Basic config flags.
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // TODO

	// Dark theming.
	ImGui::StyleColorsDark();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		auto& style                       = ImGui::GetStyle();
		style.WindowRounding              = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Attach to our windowing system.
	{
		auto window     = Window::Get();
		auto glfwWindow = window->GetWindow();

		_windowData         = std::make_unique<ImGuiWindowData>();
		_windowData->Window = glfwWindow;
		_windowData->Time   = 0.0;

		io.BackendPlatformUserData = reinterpret_cast<void*>(_windowData.get());
		io.BackendPlatformName     = "LunaGLFW";
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		// io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
#if GLFW_HAS_MOUSE_PASSTHROUGH || (GLFW_HAS_WINDOW_HOVERED && defined(_WIN32))
		io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
#endif

		io.KeyMap[ImGuiKey_Tab]         = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow]   = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow]  = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow]     = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow]   = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp]      = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown]    = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home]        = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End]         = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Insert]      = GLFW_KEY_INSERT;
		io.KeyMap[ImGuiKey_Delete]      = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace]   = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Space]       = GLFW_KEY_SPACE;
		io.KeyMap[ImGuiKey_Enter]       = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape]      = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
		io.KeyMap[ImGuiKey_A]           = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C]           = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V]           = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X]           = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y]           = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z]           = GLFW_KEY_Z;

		GLFWerrorfun prevErrorCallback                        = glfwSetErrorCallback(nullptr);
		_windowData->MouseCursors[ImGuiMouseCursor_Arrow]     = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeNS]  = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeEW]  = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_Hand]      = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#if GLFW_HAS_NEW_CURSORS
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeAll]  = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#else
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeAll]  = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		_windowData->MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
#endif
		glfwSetErrorCallback(prevErrorCallback);
	}

	// Attach to our rendering system.
	{
		auto graphics = Graphics::Get();

		_renderData = std::make_unique<ImGuiRenderData>();

		io.BackendRendererUserData = reinterpret_cast<void*>(_renderData.get());
		io.BackendRendererName     = "LunaVulkan";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		// io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

		constexpr static const char* vertGlsl = R"GLSL(
#version 450 core
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV0;
layout(location = 2) in vec4 inColor;
layout(push_constant) uniform PushConstant { vec2 Scale; vec2 Translate; float ColorCorrect; } PC;
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;
void main() {
    Out.Color = inColor;
    Out.UV = inUV0;
    gl_Position = vec4(inPosition * PC.Scale + PC.Translate, 0, 1);
}
)GLSL";
		constexpr static const char* fragGlsl = R"GLSL(
#version 450 core
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
layout(set=0, binding=0) uniform sampler2D Texture;
layout(push_constant) uniform PushConstant { vec2 Scale; vec2 Translate; float ColorCorrect; } PC;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = In.Color * texture(Texture, In.UV.st);
    if (PC.ColorCorrect == 1.0f) { outColor = pow(outColor, vec4(2.2)); }
}
)GLSL";

		_program = _device.RequestProgram(vertGlsl, fragGlsl, "ImGui Shader");

		io.Fonts->AddFontFromFileTTF(
			"Assets/Fonts/Roboto/Roboto-Bold.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.Fonts->AddFontFromFileTTF(
			"Assets/Fonts/Roboto/Roboto-Regular.ttf", 24.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.FontDefault = io.Fonts->AddFontFromFileTTF(
			"Assets/Fonts/Roboto/Roboto-SemiMedium.ttf", 15.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

		unsigned char* pixels = nullptr;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		const Vulkan::InitialImageData data{.Data = pixels};
		const auto imageCI = Vulkan::ImageCreateInfo::Immutable2D(
			vk::Format::eR8G8B8A8Unorm, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, false);
		_fontTexture = _device.CreateImage(imageCI, &data);

		const Vulkan::SamplerCreateInfo samplerCI{.MagFilter        = vk::Filter::eLinear,
		                                          .MinFilter        = vk::Filter::eLinear,
		                                          .MipmapMode       = vk::SamplerMipmapMode::eLinear,
		                                          .AddressModeU     = vk::SamplerAddressMode::eRepeat,
		                                          .AddressModeV     = vk::SamplerAddressMode::eRepeat,
		                                          .AddressModeW     = vk::SamplerAddressMode::eRepeat,
		                                          .AnisotropyEnable = false,
		                                          .MaxAnisotropy    = 1.0f,
		                                          .MinLod           = -1000.0f,
		                                          .MaxLod           = 1000.0f};
		_fontSampler = _device.RequestSampler(samplerCI);

		io.Fonts->SetTexID((reinterpret_cast<ImTextureID>(const_cast<Vulkan::ImageView*>(_fontTexture->GetView().Get()))));
	}

	// Attach to keyboard and mouse events.
	{
		auto keyboard = Keyboard::Get();
		auto mouse    = Mouse::Get();

		keyboard->OnChar += [this](char c) -> bool {
			ImGuiIO& io = ImGui::GetIO();

			io.AddInputCharacter(c);

			return io.WantCaptureKeyboard;
		};
		keyboard->OnKey += [this](Key key, InputAction action, InputMods mods, bool uiCapture) -> bool {
			ImGuiIO& io = ImGui::GetIO();

			constexpr static const int maxKey = sizeof(io.KeysDown) / sizeof(io.KeysDown[0]);
			if (static_cast<int>(key) < maxKey) {
				if (action == InputAction::Press) { io.KeysDown[static_cast<int>(key)] = true; }
				if (action == InputAction::Release) { io.KeysDown[static_cast<int>(key)] = false; }
			}
			io.KeyCtrl  = mods & InputModBits::Control;
			io.KeyShift = mods & InputModBits::Shift;
			io.KeyAlt   = mods & InputModBits::Alt;

			return false;
		};
		mouse->OnButton() += [this, mouse](MouseButton button, InputAction action, InputMods mods) -> bool {
			ImGuiIO& io = ImGui::GetIO();
			if (mouse->IsCursorHidden()) { return false; }

			constexpr static const int buttonMax =
				sizeof(_windowData->MouseJustPressed) / sizeof(_windowData->MouseJustPressed[0]);

			if (action == InputAction::Press && static_cast<int>(button) < buttonMax) {
				_windowData->MouseJustPressed[static_cast<int>(button)] = true;
			}

			return io.WantCaptureMouse;
		};
		mouse->OnScroll() += [this, mouse](const glm::dvec2& scroll) -> bool {
			ImGuiIO& io = ImGui::GetIO();
			if (mouse->IsCursorHidden()) { return false; }

			io.MouseWheelH += scroll.x;
			io.MouseWheel += scroll.y;

			return io.WantCaptureMouse;
		};
	}

	SetTheme(UI::Theme{});
}

UIManager::~UIManager() noexcept {}

void UIManager::Update() {}

void UIManager::BeginFrame() {
	ImGuiIO& io = ImGui::GetIO();
	auto window = Window::Get();
	auto mouse  = Mouse::Get();

	// Update display size and platform data.
	{
		const auto windowSize      = Window::Get()->GetSize();
		const auto framebufferSize = Window::Get()->GetFramebufferSize();
		io.DisplaySize             = ImVec2(windowSize.x, windowSize.y);
		if (windowSize.x > 0 && windowSize.y > 0) {
			io.DisplayFramebufferScale =
				ImVec2(float(framebufferSize.x) / float(windowSize.x), float(framebufferSize.y) / float(windowSize.y));
		}
		const auto now    = glfwGetTime();
		io.DeltaTime      = _windowData->Time > 0.0 ? (float) (now - _windowData->Time) : 1.0f / 60.0f;
		_windowData->Time = now;
	}

	// Update mouse.
	{
		// Update cursor icon if applicable.
		if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0 && !mouse->IsCursorHidden()) {
			const auto imguiCursor = ImGui::GetMouseCursor();
			const auto& glfwCursor = _windowData->MouseCursors[imguiCursor];
			glfwSetCursor(window->GetWindow(), glfwCursor ? glfwCursor : _windowData->MouseCursors[ImGuiMouseCursor_Arrow]);
		}

		// Update position and buttons.
		if (!mouse->IsCursorHidden()) {
			const ImVec2 prevPos             = io.MousePos;
			io.MousePos                      = ImVec2(std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
			constexpr static int buttonCount = sizeof(io.MouseDown) / sizeof(io.MouseDown[0]);
			for (int i = 0; i < buttonCount; ++i) {
				io.MouseDown[i] =
					_windowData->MouseJustPressed[i] || mouse->GetButton(static_cast<MouseButton>(i)) == InputAction::Press;
				_windowData->MouseJustPressed[i] = false;
			}
			const auto pos = mouse->GetPosition();
			io.MousePos    = ImVec2(pos.x, pos.y);
		}
	}

	ImGui::NewFrame();

	if (_dockspace) {
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("##Dockspace",
		             nullptr,
		             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar |
		               ImGuiWindowFlags_NoDocking);
		ImGuiID dockspaceID = ImGui::GetID("LunaDockspace");
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		ImGui::SetWindowSize(ImVec2(displaySize.x, displaySize.y));
		ImGui::End();

		ImGui::PopStyleVar(3);
	}
}

void UIManager::EndFrame() {
	ImGui::EndFrame();
}

void UIManager::Render(Vulkan::CommandBufferHandle& cmd) {
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();

	// Determine our window size and ensure we don't render to a minimized screen.
	const int fbWidth  = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
	const int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
	if (fbWidth <= 0 || fbHeight <= 0) { return; }

	// Set up our vertex and index buffers.
	if (drawData->TotalVtxCount > 0) {
		// Resize or create our buffers if needed.
		const size_t vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
		const size_t indexSize  = drawData->TotalIdxCount * sizeof(ImDrawIdx);
		if (!_vertexBuffer || _vertexBuffer->GetCreateInfo().Size < vertexSize) {
			_vertexBuffer = _device.CreateBuffer(
				Vulkan::BufferCreateInfo(Vulkan::BufferDomain::Host, vertexSize, vk::BufferUsageFlagBits::eVertexBuffer));
		}
		if (!_indexBuffer || _indexBuffer->GetCreateInfo().Size < indexSize) {
			_indexBuffer = _device.CreateBuffer(
				Vulkan::BufferCreateInfo(Vulkan::BufferDomain::Host, vertexSize, vk::BufferUsageFlagBits::eIndexBuffer));
		}

		// Copy our vertex and index data.
		ImDrawVert* vertices = reinterpret_cast<ImDrawVert*>(_vertexBuffer->Map());
		ImDrawIdx* indices   = reinterpret_cast<ImDrawIdx*>(_indexBuffer->Map());
		for (int i = 0; i < drawData->CmdListsCount; ++i) {
			const ImDrawList* list = drawData->CmdLists[i];
			memcpy(vertices, list->VtxBuffer.Data, list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(indices, list->IdxBuffer.Data, list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vertices += list->VtxBuffer.Size;
			indices += list->IdxBuffer.Size;
		}
		_vertexBuffer->Unmap();
		_indexBuffer->Unmap();
	}

	// Set up our render state.
	{
		Vulkan::RenderPassInfo rp = _device.GetStockRenderPass(Vulkan::StockRenderPass::ColorOnly);
		rp.Name                   = "UI Pass";
		if (_dockspace) {
			rp.ClearAttachments = 0;
			rp.ClearColors[0].setFloat32({0.0f, 0.0f, 0.0f, 1.0f});
		} else {
			rp.LoadAttachments = 1 << 0;
		}
		cmd->BeginRenderPass(rp);
		SetRenderState(cmd, drawData);
	}

	const ImVec2 clipOffset = drawData->DisplayPos;
	const ImVec2 clipScale  = drawData->FramebufferScale;

	// Render command lists.
	{
		size_t globalVtxOffset = 0;
		size_t globalIdxOffset = 0;
		for (int i = 0; i < drawData->CmdListsCount; ++i) {
			const ImDrawList* cmdList = drawData->CmdLists[i];
			for (int j = 0; j < cmdList->CmdBuffer.Size; ++j) {
				const ImDrawCmd& drawCmd = cmdList->CmdBuffer[j];

				if (drawCmd.UserCallback != nullptr) {
					if (drawCmd.UserCallback == ImDrawCallback_ResetRenderState) {
						SetRenderState(cmd, drawData);
					} else {
						drawCmd.UserCallback(cmdList, &drawCmd);
					}
				} else {
					const ImVec2 clipMin(std::max((drawCmd.ClipRect.x - clipOffset.x) * clipScale.x, 0.0f),
					                     std::max((drawCmd.ClipRect.y - clipOffset.y) * clipScale.y, 0.0f));
					const ImVec2 clipMax(
						std::min((drawCmd.ClipRect.z - clipOffset.x) * clipScale.x, static_cast<float>(fbWidth)),
						std::min((drawCmd.ClipRect.w - clipOffset.y) * clipScale.y, static_cast<float>(fbHeight)));
					if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) { continue; }

					const vk::Rect2D scissor(
						{static_cast<int32_t>(clipMin.x), static_cast<int32_t>(clipMin.y)},
						{static_cast<uint32_t>(clipMax.x - clipMin.x), static_cast<uint32_t>(clipMax.y - clipMin.y)});
					cmd->SetScissor(scissor);

					bool colorCorrect = true;
					if (drawCmd.TextureId == 0) {
						cmd->SetTexture(0, 0, *_fontTexture->GetView(), _fontSampler);
					} else {
						Vulkan::ImageView* view = reinterpret_cast<Vulkan::ImageView*>(drawCmd.TextureId);
						cmd->SetTexture(0, 0, *view, Vulkan::StockSampler::NearestClamp);
						if (Vulkan::FormatIsSrgb(view->GetCreateInfo().Format)) { colorCorrect = false; }
					}

					const float scaleX    = 2.0f / drawData->DisplaySize.x;
					const float scaleY    = 2.0f / drawData->DisplaySize.y;
					const PushConstant pc = {.ScaleX       = scaleX,
					                         .ScaleY       = scaleY,
					                         .TranslateX   = -1.0f - drawData->DisplayPos.x * scaleX,
					                         .TranslateY   = -1.0f - drawData->DisplayPos.y * scaleY,
					                         .ColorCorrect = colorCorrect ? 1.0f : 0.0f};
					cmd->PushConstants(&pc, 0, sizeof(pc));

					cmd->DrawIndexed(
						drawCmd.ElemCount, 1, drawCmd.IdxOffset + globalIdxOffset, drawCmd.VtxOffset + globalVtxOffset, 0);
				}
			}
			globalVtxOffset += cmdList->VtxBuffer.Size;
			globalIdxOffset += cmdList->IdxBuffer.Size;
		}
	}

	// Finish up.
	cmd->EndRenderPass();
}

void UIManager::SetDockspace(bool dockspace) {
	_dockspace = dockspace;
}

void UIManager::SetTheme(const UI::Theme& theme) {
	auto& colors = ImGui::GetStyle().Colors;

	colors[ImGuiCol_Header]        = theme.GroupHeader;
	colors[ImGuiCol_HeaderHovered] = theme.GroupHeaderHover;
	colors[ImGuiCol_HeaderActive]  = theme.GroupHeaderHover;

	colors[ImGuiCol_Button]        = theme.Button;
	colors[ImGuiCol_ButtonActive]  = theme.ButtonActive;
	colors[ImGuiCol_ButtonHovered] = theme.ButtonHover;

	colors[ImGuiCol_FrameBg]        = theme.PropertyField;
	colors[ImGuiCol_FrameBgActive]  = theme.PropertyFieldHover;
	colors[ImGuiCol_FrameBgHovered] = theme.PropertyFieldHover;

	colors[ImGuiCol_Tab]                = theme.Titlebar;
	colors[ImGuiCol_TabActive]          = theme.TabActive;
	colors[ImGuiCol_TabHovered]         = theme.TabHover;
	colors[ImGuiCol_TabUnfocused]       = theme.Titlebar;
	colors[ImGuiCol_TabUnfocusedActive] = theme.TabHover;

	colors[ImGuiCol_TitleBg]          = theme.Titlebar;
	colors[ImGuiCol_TitleBgActive]    = theme.Titlebar;
	colors[ImGuiCol_TitleBgCollapsed] = theme.Titlebar;

	colors[ImGuiCol_Text]      = theme.Text;
	colors[ImGuiCol_CheckMark] = theme.Text;

	colors[ImGuiCol_Separator]       = theme.BackgroundDark;
	colors[ImGuiCol_SeparatorActive] = theme.Highlight;

	colors[ImGuiCol_WindowBg] = theme.Titlebar;
	colors[ImGuiCol_ChildBg]  = theme.Background;
	colors[ImGuiCol_Border]   = theme.BackgroundDark;
}

void UIManager::RenderWindowOuterBorders(ImGuiWindow* window) const {
	struct ImGuiResizeBorderDef {
		ImVec2 InnerDir;
		ImVec2 SegmentN1, SegmentN2;
		float OuterAngle;
	};

	static const ImGuiResizeBorderDef resize_border_def[4] = {
		{ImVec2(+1, 0), ImVec2(0, 1), ImVec2(0, 0), IM_PI * 1.00f},  // Left
		{ImVec2(-1, 0), ImVec2(1, 0), ImVec2(1, 1), IM_PI * 0.00f},  // Right
		{ImVec2(0, +1), ImVec2(0, 0), ImVec2(1, 0), IM_PI * 1.50f},  // Up
		{ImVec2(0, -1), ImVec2(1, 1), ImVec2(0, 1), IM_PI * 0.50f}   // Down
	};

	auto GetResizeBorderRect = [](ImGuiWindow* window, int border_n, float perp_padding, float thickness) {
		ImRect rect = window->Rect();
		if (thickness == 0.0f) {
			rect.Max.x -= 1;
			rect.Max.y -= 1;
		}
		if (border_n == ImGuiDir_Left) {
			return ImRect(
				rect.Min.x - thickness, rect.Min.y + perp_padding, rect.Min.x + thickness, rect.Max.y - perp_padding);
		}
		if (border_n == ImGuiDir_Right) {
			return ImRect(
				rect.Max.x - thickness, rect.Min.y + perp_padding, rect.Max.x + thickness, rect.Max.y - perp_padding);
		}
		if (border_n == ImGuiDir_Up) {
			return ImRect(
				rect.Min.x + perp_padding, rect.Min.y - thickness, rect.Max.x - perp_padding, rect.Min.y + thickness);
		}
		if (border_n == ImGuiDir_Down) {
			return ImRect(
				rect.Min.x + perp_padding, rect.Max.y - thickness, rect.Max.x - perp_padding, rect.Max.y + thickness);
		}
		IM_ASSERT(0);
		return ImRect();
	};

	ImGuiContext& g   = *GImGui;
	float rounding    = window->WindowRounding;
	float border_size = 1.0f;  // window->WindowBorderSize;
	if (border_size > 0.0f && !(window->Flags & ImGuiWindowFlags_NoBackground))
		window->DrawList->AddRect(window->Pos,
		                          {window->Pos.x + window->Size.x, window->Pos.y + window->Size.y},
		                          ImGui::GetColorU32(ImGuiCol_Border),
		                          rounding,
		                          0,
		                          border_size);

	int border_held = window->ResizeBorderHeld;
	if (border_held != -1) {
		const ImGuiResizeBorderDef& def = resize_border_def[border_held];
		ImRect border_r                 = GetResizeBorderRect(window, border_held, rounding, 0.0f);
		ImVec2 p1                       = ImLerp(border_r.Min, border_r.Max, def.SegmentN1);
		const float offsetX             = def.InnerDir.x * rounding;
		const float offsetY             = def.InnerDir.y * rounding;
		p1.x += 0.5f + offsetX;
		p1.y += 0.5f + offsetY;

		ImVec2 p2 = ImLerp(border_r.Min, border_r.Max, def.SegmentN2);
		p2.x += 0.5f + offsetX;
		p2.y += 0.5f + offsetY;

		window->DrawList->PathArcTo(p1, rounding, def.OuterAngle - IM_PI * 0.25f, def.OuterAngle);
		window->DrawList->PathArcTo(p2, rounding, def.OuterAngle, def.OuterAngle + IM_PI * 0.25f);
		window->DrawList->PathStroke(
			ImGui::GetColorU32(ImGuiCol_SeparatorActive), 0, ImMax(2.0f, border_size));  // Thicker than usual
	}
	if (g.Style.FrameBorderSize > 0 && !(window->Flags & ImGuiWindowFlags_NoTitleBar) && !window->DockIsActive) {
		float y = window->Pos.y + window->TitleBarHeight() - 1;
		window->DrawList->AddLine(ImVec2(window->Pos.x + border_size, y),
		                          ImVec2(window->Pos.x + window->Size.x - border_size, y),
		                          ImGui::GetColorU32(ImGuiCol_Border),
		                          g.Style.FrameBorderSize);
	}
}

void UIManager::SetRenderState(Vulkan::CommandBufferHandle& cmd, ImDrawData* drawData) const {
	if (drawData->TotalVtxCount == 0) { return; }

	cmd->SetProgram(_program);
	cmd->SetTransparentSpriteState();
	cmd->SetVertexAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, pos));
	cmd->SetVertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, uv));
	cmd->SetVertexAttribute(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(ImDrawVert, col));
	cmd->SetVertexBinding(0, *_vertexBuffer, 0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex);
	cmd->SetIndexBuffer(*_indexBuffer, 0, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
}
}  // namespace Luna
