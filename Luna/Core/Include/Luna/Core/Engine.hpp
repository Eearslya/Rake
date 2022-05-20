#pragma once

#include <Luna/Utility/Module.hpp>
#include <Luna/Utility/Time.hpp>
#include <memory>

namespace Luna {
class App;

class Engine {
 public:
	Engine();
	~Engine() noexcept;

	static Engine* Get() {
		return _instance;
	}

	Utility::Time GetFrameDelta() const {
		return _frameDelta.Get();
	}
	uint32_t GetFPS() const {
		return _fps.Get();
	}
	uint32_t GetFPSLimit() const {
		return _fpsLimit;
	}
	Utility::Time GetUpdateDelta() const {
		return _updateDelta.Get();
	}
	uint32_t GetUPS() const {
		return _ups.Get();
	}
	uint32_t GetUPSLimit() const {
		return _upsLimit;
	}

	int Run();
	void Shutdown();

	void SetApp(App* app);
	void SetFPSLimit(uint32_t limit);
	void SetUPSLimit(uint32_t limit);

 private:
	static Engine* _instance;

	App* _app = nullptr;
	std::multimap<Utility::Module::StageIndex, Utility::Module*> _moduleMap;
	std::vector<std::unique_ptr<Utility::Module>> _modules;
	bool _running = false;

	Utility::ElapsedTime _frameDelta;
	Utility::IntervalCounter _frameLimiter;
	Utility::UpdatesPerSecond _fps;
	uint32_t _fpsLimit = 60;

	Utility::ElapsedTime _updateDelta;
	Utility::IntervalCounter _updateLimiter;
	Utility::UpdatesPerSecond _ups;
	uint32_t _upsLimit = 100;
};
}  // namespace Luna
