#include <Luna/Core/App.hpp>
#include <Luna/Core/Engine.hpp>
#include <Luna/Utility/Log.hpp>
#include <stdexcept>

namespace Luna {
Engine* Engine::_instance = nullptr;

Engine::Engine() {
	_instance = this;

	Log::Initialize();
#ifdef LUNA_DEBUG
	Log::SetLevel(Log::Level::Trace);
#endif

	Log::Info("Engine", "Initializing Luna engine.");

	std::vector<Utility::TypeID> createdModules;
	uint8_t retryCount = 32;
	while (true) {
		bool postponed = false;

		for (const auto& [moduleID, moduleInfo] : Utility::Module::Registry()) {
			if (std::find(createdModules.begin(), createdModules.end(), moduleID) != createdModules.end()) { continue; }

			bool thisPostponed = false;
			for (const auto& dependencyID : moduleInfo.Dependencies) {
				if (std::find(createdModules.begin(), createdModules.end(), dependencyID) == createdModules.end()) {
					thisPostponed = true;
					break;
				}
			}

			if (thisPostponed) {
				postponed = true;
				continue;
			}

			try {
				Log::Debug("Engine", "Initializing Engine module '{}'.", moduleInfo.Name);

				auto&& module = moduleInfo.Create();
				_moduleMap.emplace(Utility::Module::StageIndex(moduleInfo.Stage, moduleID), module.get());
				_modules.push_back(std::move(module));
				createdModules.emplace_back(moduleID);
			} catch (const std::exception& e) {
				Log::Fatal("Engine", "Failed to initialize Engine module '{}': {}", moduleInfo.Name, e.what());
				throw e;
			}
		}

		if (postponed) {
			if (--retryCount == 0) {
				Log::Fatal("Engine",
				           "Failed to initialize Engine modules. A dependency is missing or a circular dependency is present.");
				throw std::runtime_error("Failed to initialize engine modules!");
			}
		} else {
			break;
		}
	}

	Log::Debug("Engine", "All engine modules initialized.");

	SetFPSLimit(_fpsLimit);
	SetUPSLimit(_upsLimit);
}

Engine::~Engine() noexcept {
	for (auto it = _modules.rbegin(); it != _modules.rend(); ++it) { it->reset(); }
	_modules.clear();
	_moduleMap.clear();
	Log::Shutdown();
}

int Engine::Run() {
	const auto UpdateStage = [this](Utility::Module::Stage stage) {
		for (auto& [stageIndex, module] : _moduleMap) {
			if (stageIndex.first == stage) { module->Update(); }
		}
	};

	_running = true;
	while (_running) {
		UpdateStage(Utility::Module::Stage::Always);

		_updateLimiter.Update();
		if (_updateLimiter.Get() > 0) {
			_ups.Update();
			_updateDelta.Update();

			if (_app) {
				if (!_app->_started) {
					_app->Start();
					_app->_started = true;
				}

				try {
					_app->Update();
				} catch (const std::exception& e) {
					Log::Fatal("Engine", "Caught fatal error when updating application: {}", e.what());
					_running = false;
					break;
				}
			}

			try {
				UpdateStage(Utility::Module::Stage::Pre);
				UpdateStage(Utility::Module::Stage::Normal);
				UpdateStage(Utility::Module::Stage::Post);
			} catch (const std::exception& e) {
				Log::Fatal("Engine", "Caught fatal error when updating engine modules: {}", e.what());
				_running = false;
				break;
			}
		}

		_frameLimiter.Update();
		if (_frameLimiter.Get() > 0) {
			_fps.Update();
			_frameDelta.Update();

			try {
				UpdateStage(Utility::Module::Stage::Render);
			} catch (const std::exception& e) {
				Log::Fatal("Engine", "Caught fatal error when rendering: {}", e.what());
				_running = false;
				break;
			}
		}
	}

	if (_app) {
		_app->Stop();
		_app->_started = false;
	}

	return 0;
}

void Engine::Shutdown() {
	_running = false;
}

void Engine::SetApp(App* app) {
	if (_app) {
		_app->Stop();
		_app->_started = false;
	}
	_app = app;
}

void Engine::SetFPSLimit(uint32_t limit) {
	_fpsLimit = limit;
	_frameLimiter.SetInterval(Utility::Time::Seconds(1.0f / limit));
}

void Engine::SetUPSLimit(uint32_t limit) {
	_upsLimit = limit;
	_updateLimiter.SetInterval(Utility::Time::Seconds(1.0f / limit));
}
}  // namespace Luna
