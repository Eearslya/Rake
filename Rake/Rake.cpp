#include "Rake.hpp"

#include <Luna.hpp>
#include <Luna/Graphics/Vulkan/Buffer.hpp>
#include <Luna/Graphics/Vulkan/CommandBuffer.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/Image.hpp>
#include <Luna/Utility/Log.hpp>
#include <Luna/Utility/Time.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "RenderMessages.hpp"
#include "Sphere.hpp"
#include "Tracer.hpp"
#include "World.hpp"

using namespace Luna;

Rake::Rake() : App("Rake") {}

Rake::~Rake() noexcept = default;

void Rake::Start() {
	Window::Get()->OnClosed += []() { Engine::Get()->Shutdown(); };
	Window::Get()->Maximize();
	Window::Get()->SetTitle("Rake");

	_tracer = std::make_unique<Tracer>();

	Graphics::Get()->OnRender += [this]() { Render(); };

	const auto CreateWorld = [this](const std::string& name) -> World& {
		auto& world = _worlds.emplace_back(std::make_shared<World>(name));
		return *world;
	};

	{
		auto& world               = CreateWorld("World");
		world.CameraPos           = Point3(0.0, 0.0, 0.0);
		world.CameraTarget        = Point3(0.0, 0.0, -1.0);
		world.CameraFocusDistance = 1.0;
		world.Objects.Add<Sphere>(Point3(0, 0, -1), 0.5);
		world.Objects.Add<Sphere>(Point3(0, -100.5, -1), 100);
	}
}

void Rake::Update() {
	_renderTime.Update();
	if (_dirty) { Invalidate(); }
}

void Rake::Stop() {
	_tracer.reset();
	_copyBuffer.Reset();
	_renderImage.Reset();
}

void Rake::Render() {
	auto& device = Graphics::Get()->GetDevice();

	auto cmdBuf = device.RequestCommandBuffer(Vulkan::CommandBufferType::Generic, "Main Command Buffer");

	if (_tracer->Status.front()) {
		RenderStatus status = *_tracer->Status.front();
		_tracer->Status.pop();

		_samplesCompleted = status.FinishedSamples;
		_raysCompleted    = status.TotalRaycasts;
	}
	if (_tracer->Completes.front()) {
		_tracer->Completes.pop();
		_renderTime.Stop();
	}

	bool gotResults = false;
	int resultLimit = 8;
	while (resultLimit > 0 && _tracer->Results.front()) {
		RenderResult result = std::move(*_tracer->Results.front());
		_tracer->Results.pop();
		--resultLimit;
		gotResults = true;

		const auto scale = 1.0 / result.SampleCount;
		for (auto& pixel : result.Pixels) {
			pixel.r = glm::sqrt(scale * pixel.r);
			pixel.g = glm::sqrt(scale * pixel.g);
			pixel.b = glm::sqrt(scale * pixel.b);
		}

		Color* copyBuffer  = reinterpret_cast<Color*>(_copyBuffer->Map());
		Color* start       = copyBuffer + (result.MinY * result.Width);
		const size_t bytes = (result.MaxY - result.MinY) * result.Width * sizeof(Color);
		memcpy(start, result.Pixels.data(), bytes);
	}
	if (gotResults) {
		cmdBuf->ImageBarrier(*_renderImage,
		                     vk::ImageLayout::eUndefined,
		                     vk::ImageLayout::eTransferDstOptimal,
		                     vk::PipelineStageFlagBits::eTopOfPipe,
		                     vk::AccessFlagBits::eNone,
		                     vk::PipelineStageFlagBits::eTransfer,
		                     vk::AccessFlagBits::eTransferWrite);

		const vk::BufferImageCopy copy(0,
		                               0,
		                               0,
		                               vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
		                               vk::Offset3D(0, 0, 0),
		                               vk::Extent3D(_viewportSize.x, _viewportSize.y, 0));
		cmdBuf->CopyBufferToImage(*_renderImage, *_copyBuffer, {copy});

		cmdBuf->ImageBarrier(*_renderImage,
		                     vk::ImageLayout::eTransferDstOptimal,
		                     vk::ImageLayout::eShaderReadOnlyOptimal,
		                     vk::PipelineStageFlagBits::eTransfer,
		                     vk::AccessFlagBits::eTransferWrite,
		                     vk::PipelineStageFlagBits::eFragmentShader,
		                     vk::AccessFlagBits::eShaderRead);
	}

	UIManager::Get()->BeginFrame();
	RenderRakeUI();
	UIManager::Get()->Render(cmdBuf);
	UIManager::Get()->EndFrame();

	device.Submit(cmdBuf);
}

void Rake::RequestCancel() {
	RenderCancel cancel;
	const bool cancelled = _tracer->Cancels.try_push(cancel);
	_renderTime.Stop();
}

void Rake::RequestTrace(bool preview) {
	auto& device = Graphics::Get()->GetDevice();

	RenderRequest request{.ImageSize   = _viewportSize,
	                      .SampleCount = preview ? _previewSamples : _samplesPerPixel,
	                      .World       = _worlds[_currentWorld]};
	_tracer->Requests.push(request);
	std::vector<Color> pixels(_viewportSize.x * _viewportSize.y);
	std::fill(pixels.begin(), pixels.end(), Color(0.0f));

	const Vulkan::ImageCreateInfo imageCI = Vulkan::ImageCreateInfo::Immutable2D(
		vk::Format::eR32G32B32Sfloat, vk::Extent2D(_viewportSize.x, _viewportSize.y), false);
	const Vulkan::InitialImageData initial{.Data = pixels.data(), .RowLength = 0, .ImageHeight = 0};
	_renderImage = device.CreateImage(imageCI, &initial);

	const Vulkan::BufferCreateInfo bufferCI(Vulkan::BufferDomain::Host,
	                                        _viewportSize.x * _viewportSize.y * sizeof(Color),
	                                        vk::BufferUsageFlagBits::eTransferSrc);
	_copyBuffer = device.CreateBuffer(bufferCI);

	_renderTime.Start();
	_samplesRequested = request.SampleCount;
}

void Rake::Invalidate() {
	RequestTrace(true);
	_dirty = false;
}

void Rake::RenderRakeUI() {
	RenderControls();
	RenderDockspace();
	RenderViewport();
	RenderWorld();
	ImGui::ShowDemoWindow();
}

void Rake::RenderControls() {
	ImGuiViewport* viewport  = ImGui::GetMainViewport();
	const bool tracerRunning = _tracer->IsRunning();

	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 64.0f));
	if (ImGui::Begin("Controls##Controls",
	                 nullptr,
	                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking)) {
		if (ImGui::BeginTable("ControlsTable", 3, ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 48.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 160.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::BeginGroup();
			{
				if (tracerRunning) {
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(64, 32, 32, 255));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(192, 64, 64, 255));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 64, 64, 255));
					if (ImGui::ButtonEx("Cancel", ImVec2(48.0f, 48.0f))) { RequestCancel(); }
					ImGui::PopStyleColor(3);
				} else {
					if (ImGui::ButtonEx("Start", ImVec2(48.0f, 48.0f))) { RequestTrace(); }
				}
			}
			ImGui::EndGroup();

			ImGui::TableSetColumnIndex(1);
			ImGui::BeginGroup();
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0, 0.0));

				const auto renderTime        = _renderTime.Get();
				const auto renderTimeSeconds = renderTime.AsSeconds<float>();
				std::string renderTimeStr;
				if (renderTimeSeconds > 10.0f) {
					renderTimeStr = fmt::format("Render Time: {:.1f}s", renderTimeSeconds);
				} else {
					renderTimeStr = fmt::format("Render Time: {:.2f}ms", renderTime.AsMilliseconds<float>());
				}
				ImGui::Text("%s", renderTimeStr.c_str());

				const std::string progressStr = fmt::format("Progress: {} / {}", _samplesCompleted, _samplesRequested);
				ImGui::Text("%s", progressStr.c_str());

				const uint64_t raysPerSecond =
					renderTimeSeconds > 0.0f ? std::floor(float(_raysCompleted) / renderTimeSeconds) : 0ull;
				const std::string rpsStr = fmt::format(std::locale("en_US.UTF-8"), "RPS: {:L}", raysPerSecond);
				ImGui::Text("%s", rpsStr.c_str());

				ImGui::PopStyleVar();
			}
			ImGui::EndGroup();

			ImGui::TableSetColumnIndex(2);
			if (tracerRunning) { ImGui::BeginDisabled(); }
			ImGui::BeginGroup();
			{
				ImGui::SetNextItemWidth(48.0f);
				ImGui::InputScalar("###SamplesPerPixel", ImGuiDataType_U32, &_samplesPerPixel, nullptr, nullptr, "%lu");
				_samplesPerPixel     = std::max(_samplesPerPixel, 1u);
				const auto labelSize = ImGui::CalcTextSize("Render");
				const auto padding   = (48.0f - labelSize.x) / 2.0f;
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padding);
				ImGui::Text("Render");
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				ImGui::SetNextItemWidth(48.0f);
				if (ImGui::InputScalar("###PreviewSamples", ImGuiDataType_U32, &_previewSamples, nullptr, nullptr, "%lu")) {
					Invalidate();
				}
				const auto labelSize = ImGui::CalcTextSize("Preview");
				const auto padding   = (48.0f - labelSize.x) / 2.0f;
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padding);
				ImGui::Text("Preview");
			}
			ImGui::EndGroup();
			if (tracerRunning) { ImGui::EndDisabled(); }

			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void Rake::RenderDockspace() {
	ImGuiStyle& style = ImGui::GetStyle();

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + 64.0f));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - 64.0f));
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

void Rake::RenderViewport() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	const std::string title = fmt::format("Render Result ({}x{})###Viewport", _viewportSize.x, _viewportSize.y);
	const bool draw         = ImGui::Begin(title.c_str());
	ImGui::PopStyleVar();
	if (draw) {
		const auto viewportSize = ImGui::GetContentRegionAvail();
		_viewportSize           = glm::uvec2(viewportSize.x, viewportSize.y);
		if (_renderImage) {
			ImGui::Image(reinterpret_cast<ImTextureID>(const_cast<Luna::Vulkan::ImageView*>(_renderImage->GetView().Get())),
			             viewportSize);
		}
	}
	ImGui::End();
}

void Rake::RenderWorld() {
	auto& world              = _worlds[_currentWorld];
	const bool tracerRunning = _tracer->IsRunning();

	if (ImGui::Begin("World")) {
		{
			std::vector<const char*> worldNames;
			for (const auto& w : _worlds) { worldNames.push_back(w->Name.c_str()); }
			ImGui::Combo(
				"Active", reinterpret_cast<int*>(&_currentWorld), worldNames.data(), static_cast<int>(worldNames.size()));
		}
		ImGui::Separator();

		if (tracerRunning) { ImGui::BeginDisabled(); }

		ImGui::ColorEdit3("Sky Color", glm::value_ptr(world->Sky));

		glm::vec3 camPos = world->CameraPos;
		if (ImGui::DragFloat3("Camera Position", glm::value_ptr(camPos), 0.1f, 0.0f, 0.0f, "%.2f")) {
			world->CameraPos = camPos;
		}

		glm::vec3 camTarget = world->CameraTarget;
		if (ImGui::DragFloat3("Camera Target", glm::value_ptr(camTarget), 0.1f, 0.0f, 0.0f, "%.2f")) {
			world->CameraTarget = camTarget;
		}

		float vFov = world->VerticalFOV;
		if (ImGui::DragFloat("Vertical FOV", &vFov, 0.1f, 0.0f, 0.0f, "%.2f")) { world->VerticalFOV = vFov; }

		float camAperture = world->CameraAperture;
		if (ImGui::DragFloat("Camera Aperture", &camAperture, 0.1f, 0.0f, 0.0f, "%.2f")) {
			world->CameraAperture = camAperture;
		}

		float camFocus = world->CameraFocusDistance;
		if (ImGui::DragFloat("Camera Focus", &camFocus, 0.1f, 0.0f, 0.0f, "%.2f")) {
			world->CameraFocusDistance = camFocus;
		}

		if (ImGui::ButtonEx("Refresh", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) { _dirty = true; }

		if (tracerRunning) { ImGui::EndDisabled(); }
	}
	ImGui::End();
}
