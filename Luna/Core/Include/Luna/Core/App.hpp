#pragma once

#include <Luna/Utility/Delegate.hpp>
#include <string>

namespace Luna {
class App : public Utility::Observer {
	friend class Engine;

 public:
	explicit App(const std::string& name) : _name(name) {}
	virtual ~App() noexcept = default;

	virtual void Start()  = 0;
	virtual void Stop()   = 0;
	virtual void Update() = 0;

	const std::string& GetName() const {
		return _name;
	}

 private:
	const std::string _name;
	bool _started = false;
};
}  // namespace Luna
