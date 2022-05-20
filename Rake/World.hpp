#pragma once

#include <string>

#include "HittableList.hpp"

struct World {
	World(const std::string& name) : Name(name) {}

	std::string Name;
	HittableList Objects;
	double VerticalFOV         = 90.0f;
	Point3 CameraPos           = Point3(0.0);
	Point3 CameraTarget        = Point3(0.0, 0.0, -1.0);
	double CameraAperture      = 0.01;
	double CameraFocusDistance = 100.0;
	Color Sky                  = Color(0.7, 0.8, 1.0);
};
