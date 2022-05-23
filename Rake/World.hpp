#pragma once

#include <string>

struct World {
	World(const std::string& name) : Name(name) {}

	std::string Name;
};
