#pragma once

#include <Luna/Platform/Keyboard.hpp>
#include <Luna/Platform/Mouse.hpp>

namespace Luna {
class Keyboard;
class Mouse;

class Input : public Utility::Module::Registrar<Input> {
	static inline const bool Registered = Register("Input", Stage::Normal, Depends<Keyboard, Mouse>());

 public:
	Input();

	virtual void Update() override;
};
}  // namespace Luna
