#include "Rake.hpp"

#include <stb_image_write.h>

#include <Luna.hpp>
#include <Luna/Graphics/Vulkan/Buffer.hpp>
#include <Luna/Graphics/Vulkan/CommandBuffer.hpp>
#include <Luna/Graphics/Vulkan/Device.hpp>
#include <Luna/Graphics/Vulkan/Image.hpp>
#include <Luna/Utility/Log.hpp>
#include <Luna/Utility/Time.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "CheckerTexture.hpp"
#include "ImageTexture.hpp"
#include "Materials/DielectricMaterial.hpp"
#include "Materials/DiffuseLightMaterial.hpp"
#include "Materials/GradientSkyMaterial.hpp"
#include "Materials/LambertianMaterial.hpp"
#include "Materials/MetalMaterial.hpp"
#include "Materials/SolidSkyMaterial.hpp"
#include "Plane.hpp"
#include "Random.hpp"
#include "RenderMessages.hpp"
#include "SolidTexture.hpp"
#include "Sphere.hpp"
#include "Tracer.hpp"
#include "World.hpp"

using namespace Luna;

Rake::Rake() : App("Rake") {}

Rake::~Rake() noexcept {
	if (_exportThread.joinable()) { _exportThread.join(); }
}

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
		world.Sky                 = std::make_shared<GradientSkyMaterial>(Color(1.0), Color(0.5, 0.7, 1.0), 0.5);
		world.CameraPos           = Point3(0.0, 0.0, 0.0);
		world.CameraTarget        = Point3(0.0, 0.0, -1.0);
		world.CameraFocusDistance = 1.0;
		world.VerticalFOV         = 100;
		auto ground               = std::make_shared<LambertianMaterial>(Color(0.3, 0.3, 0.8));
		auto center               = std::make_shared<LambertianMaterial>(Color(0.3, 0.8, 0.3));
		auto left                 = std::make_shared<DielectricMaterial>(1.5);
		auto right                = std::make_shared<MetalMaterial>(Color(0.8, 0.6, 0.2), 1.0);
		world.Objects.Add<Sphere>(Point3(0, -100.5, -1), 100, ground);
		world.Objects.Add<Sphere>(Point3(0, 0, -1), 0.5, center);
		world.Objects.Add<Sphere>(Point3(-1, 0, -1), 0.5, left);
		world.Objects.Add<Sphere>(Point3(-1, 0, -1), -0.45, left);
		world.Objects.Add<Sphere>(Point3(1, 0, -1), 0.5, right);
	}

	{
		auto& world = CreateWorld("Raytracing In One Weekend");
		// world.Sky          = std::make_shared<GradientSkyMaterial>(Color(1.0) * 0.2f, Color(0.5, 0.7, 1.0) * 0.2f, 0.5);
		world.Sky = std::make_shared<SolidSkyMaterial>(std::make_shared<ImageTexture>("Assets/Textures/TokyoBigSight.hdr"));
		world.CameraPos           = Point3(13.0, 2.0, 5.0);
		world.CameraTarget        = Point3(0.0, 0.0, 0.0);
		world.CameraFocusDistance = 12.0;
		world.CameraAperture      = 0.1;
		world.VerticalFOV         = 20;

		auto sun          = std::make_shared<DiffuseLightMaterial>(Color(0.5, 0.9, 0.9) * 30.0f);
		auto checker      = std::make_shared<CheckerTexture>(Color(0.2), Color(0.36, 0.0, 0.63), glm::vec2(Pi));
		auto earth        = std::make_shared<ImageTexture>("Assets/Textures/Earth.jpg");
		auto ground       = std::make_shared<LambertianMaterial>(checker);
		auto center       = std::make_shared<DielectricMaterial>(1.5);
		auto left         = std::make_shared<LambertianMaterial>(earth);
		auto right        = std::make_shared<MetalMaterial>(Color(0.7, 0.6, 0.5), 0.0);
		const auto sunPos = RandomInHemisphere(Vector3(0, 1, 0)) * 250.0;
		world.Objects.Add<Sphere>(sunPos, 50, sun);
		world.Objects.Add<XZPlane>(0.0, ground);
		world.Objects.Add<Sphere>(Point3(0, 1, 0), 1, center);
		world.Objects.Add<Sphere>(Point3(-4, 1, 0), 1, left);
		world.Objects.Add<Sphere>(Point3(4, 1, 0), 1, right);

		for (int x = -11; x < 11; x++) {
			for (int y = -11; y < 11; y++) {
				const auto randomMat = RandomDouble();
				const Point3 center(x + 0.9 * RandomDouble(), 0.2, y + 0.9 * RandomDouble());

				if (glm::length(center - Point3(4, 0.2, 0)) > 0.9) {
					std::shared_ptr<IMaterial> material;
					if (randomMat < 0.3) {
						const auto albedo = RandomColor() * RandomColor();
						material          = std::make_shared<LambertianMaterial>(albedo);
					} else if (randomMat < 0.7) {
						const auto albedoA = RandomColor() * RandomColor();
						const auto albedoB = RandomColor() * RandomColor();
						material           = std::make_shared<LambertianMaterial>(
              std::make_shared<CheckerTexture>(albedoA, albedoB, glm::vec2(30.0f, 15.0f)));
					} else if (randomMat < 0.8) {
						const auto albedo = RandomColor() * RandomColor();
						material          = std::make_shared<DiffuseLightMaterial>(albedo * 5.0f);
					} else if (randomMat < 0.95) {
						const auto albedo    = RandomColor(0.5, 1.0);
						const auto roughness = RandomDouble(0.0, 0.5);
						material             = std::make_shared<MetalMaterial>(albedo, roughness);
					} else {
						material = std::make_shared<DielectricMaterial>(1.5);
					}

					world.Objects.Add<Sphere>(center, 0.2, material);
				}
			}
		}
	}
}

void Rake::Update() {
	_exportTimer.Update();
	_tracer->Update();
	if (_dirty) { Invalidate(); }
	if (_exportThread.joinable()) { _exportThread.join(); }
}

void Rake::Stop() {
	_tracer.reset();
	_copyBuffer.Reset();
	_renderImage.Reset();
}

void Rake::Render() {
	auto& device = Graphics::Get()->GetDevice();

	auto cmdBuf = device.RequestCommandBuffer(Vulkan::CommandBufferType::Generic, "Main Command Buffer");

	const bool renderUpdated = _tracer->UpdatePixels(_pixels);
	if (renderUpdated) {
		for (auto& pixel : _pixels) {
			pixel.r = glm::sqrt(pixel.r);
			pixel.g = glm::sqrt(pixel.g);
			pixel.b = glm::sqrt(pixel.b);
		}
		Color* pixelData = reinterpret_cast<Color*>(_copyBuffer->Map());
		memcpy(pixelData, _pixels.data(), _copyBuffer->GetCreateInfo().Size);

		const auto samples = _tracer->GetCompletedSamples();
		auto nextExport    = _lastExport + _autoExport;
		nextExport -= nextExport % _autoExport;
		nextExport    = _lastExport == 0 ? 1 : nextExport;
		bool doExport = samples >= nextExport && samples > _lastExport;
		if (doExport) {
			_samplesCompleted = nextExport;
			_lastExport       = nextExport;
			Export();
		}

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

void Rake::Export() {
	if (_exporting) { return; }
	_exporting = true;
	_exportTimer.Start();
	Color* pixels              = reinterpret_cast<Color*>(_copyBuffer->Map());
	const size_t pixelCount    = _viewportSize.x * _viewportSize.y;
	const std::string filename = fmt::format("{}-{}.png", _worlds[_currentWorld]->Name, _samplesCompleted);
	Log::Info("Rake", "Exporting render result {}.", filename);
	_exportThread = std::thread(
		[this](const std::string& filename, const glm::uvec2& size, const std::vector<Color> pixels) {
			ExportThread(filename, size, pixels);
		},
		filename,
		_viewportSize,
		std::vector<Color>(pixels, pixels + pixelCount));
}

void Rake::RequestCancel() {
	if (_tracer->CancelTrace()) { _renderTime.Stop(); }
}

void Rake::RequestTrace(bool preview) {
	auto& device = Graphics::Get()->GetDevice();

	const auto samplesRequested = preview ? _previewSamples : _samplesPerPixel;

	if (_tracer->StartTrace(_viewportSize, samplesRequested, _worlds[_currentWorld])) {
		_pixels.resize(_viewportSize.x * _viewportSize.y);
		std::fill(_pixels.begin(), _pixels.end(), Color(0.0f));

		const Vulkan::ImageCreateInfo imageCI = Vulkan::ImageCreateInfo::Immutable2D(
			vk::Format::eR32G32B32Sfloat, vk::Extent2D(_viewportSize.x, _viewportSize.y), false);
		const Vulkan::InitialImageData initial{.Data = _pixels.data(), .RowLength = 0, .ImageHeight = 0};
		_renderImage = device.CreateImage(imageCI, &initial);

		const Vulkan::BufferCreateInfo bufferCI(Vulkan::BufferDomain::Host,
		                                        _viewportSize.x * _viewportSize.y * sizeof(Color),
		                                        vk::BufferUsageFlagBits::eTransferSrc);
		_copyBuffer = device.CreateBuffer(bufferCI);

		_samplesRequested = samplesRequested;
		_samplesCompleted = 0;
		_renderTime.Start();
		_threadStatus.clear();
		_lastExport = 0;
	} else {
		Log::Warning("Rake", "Failed to request raytrace task!");
	}
}

void Rake::Invalidate() {
	_dirty = false;
	RequestTrace(true);
}

void Rake::RenderRakeUI() {
	RenderControls();
	RenderDockspace();
	RenderViewport();
	RenderWorld();
	ImGui::ShowDemoWindow();

	ImGui::Begin("Debug");
	for (size_t t = 0; t < _threadStatus.size(); ++t) {
		const std::string status = fmt::format("Thread {}: {}", t, _threadStatus[t]);
		ImGui::Text("%s", status.c_str());
	}
	ImGui::End();
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
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 48.0f + 4.0f + 48.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 160.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::BeginGroup();
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0, 4.0));

				if (tracerRunning) {
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(64, 32, 32, 255));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(192, 64, 64, 255));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 64, 64, 255));
					if (ImGui::ButtonEx("Cancel", ImVec2(48.0f, 48.0f))) { RequestCancel(); }
					ImGui::PopStyleColor(3);
				} else {
					if (ImGui::ButtonEx("Start", ImVec2(48.0f, 48.0f))) { RequestTrace(); }
				}

				ImGui::SameLine();
				ImGui::BeginGroup();
				if (!CanExport()) { ImGui::BeginDisabled(); }
				if (_exporting) {
					constexpr float interval = 0.2f;
					const auto intervals     = _exportTimer.Get().AsSeconds<float>() * (1.0f / interval);
					const auto count         = (static_cast<int>(intervals) % 5) + 1;
					const auto dots          = fmt::format("{:.>{}}", "", count);
					ImGui::ButtonEx(dots.c_str(), ImVec2(48.0f, 24.0f));
				} else {
					if (ImGui::ButtonEx("Export", ImVec2(48.0f, 24.0f))) { Export(); }
				}
				ImGui::SetNextItemWidth(48.0f);
				ImGui::InputScalar("###AutoExport", ImGuiDataType_U32, &_autoExport, nullptr, nullptr, "%u");
				if (!CanExport()) { ImGui::EndDisabled(); }
				ImGui::EndGroup();

				ImGui::PopStyleVar();
			}
			ImGui::EndGroup();

			ImGui::TableSetColumnIndex(1);
			ImGui::BeginGroup();
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0, 0.0));

				const auto renderTime        = _tracer->GetElapsedTime();
				const auto renderTimeSeconds = renderTime.AsSeconds<float>();
				std::string renderTimeStr;
				if (renderTimeSeconds > 10.0f) {
					renderTimeStr = fmt::format("Render Time: {:.1f}s", renderTimeSeconds);
				} else {
					renderTimeStr = fmt::format("Render Time: {:.2f}ms", renderTime.AsMilliseconds<float>());
				}
				ImGui::Text("%s", renderTimeStr.c_str());

				const std::string progressStr =
					fmt::format("Progress: {} / {}", _tracer->GetCompletedSamples(), _samplesRequested);
				ImGui::Text("%s", progressStr.c_str());

				const uint64_t raysPerSecond =
					renderTimeSeconds > 0.0f ? std::floor(float(_tracer->GetRaycastCount()) / renderTimeSeconds) : 0ull;
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
				ImGui::InputScalar("###PreviewSamples", ImGuiDataType_U32, &_previewSamples, nullptr, nullptr, "%lu");
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
		} else {
			_dirty = true;
		}
	}
	ImGui::End();
}

void Rake::RenderWorld() {
	auto& world              = _worlds[_currentWorld];
	const bool tracerRunning = _tracer->IsRunning();

	if (ImGui::Begin("World")) {
		if (tracerRunning) { ImGui::BeginDisabled(); }

		{
			std::vector<const char*> worldNames;
			for (const auto& w : _worlds) { worldNames.push_back(w->Name.c_str()); }
			if (ImGui::Combo(
						"Active", reinterpret_cast<int*>(&_currentWorld), worldNames.data(), static_cast<int>(worldNames.size()))) {
				Invalidate();
			}
		}
		ImGui::Separator();

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

bool Rake::CanExport() const {
	return !_tracer->IsRunning() && _copyBuffer && !_exporting;
}

void Rake::ExportThread(const std::string& filename, const glm::uvec2& viewportSize, const std::vector<Color> pixels) {
	const auto pixelCount = viewportSize.x * viewportSize.y;
	std::vector<uint32_t> rgba(pixelCount, 0xff000000);
	for (uint64_t pixel = 0; pixel < pixelCount; ++pixel) {
		const auto& p    = pixels[pixel];
		const uint32_t r = static_cast<uint8_t>(glm::clamp(p.r, 0.0f, 1.0f) * 255.999f);
		const uint32_t g = static_cast<uint8_t>(glm::clamp(p.g, 0.0f, 1.0f) * 255.999f) << 8;
		const uint32_t b = static_cast<uint8_t>(glm::clamp(p.b, 0.0f, 1.0f) * 255.999f) << 16;
		rgba[pixel] |= r | g | b;
	}
	stbi_write_png(filename.c_str(), viewportSize.x, viewportSize.y, 4, rgba.data(), sizeof(uint32_t) * viewportSize.x);
	_exporting = false;
	_exportTimer.Stop();
}
