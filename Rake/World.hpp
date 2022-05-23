#pragma once

#include <memory>
#include <string>

#include "BVHNode.hpp"
#include "HittableList.hpp"

class ISkyMaterial;

struct World {
	World(const std::string& name) : Name(name) {}

	void ConstructBVH() {
		BVH = std::make_shared<BVHNode>(Objects);
	}

	std::string Name;
	HittableList Objects;
	std::shared_ptr<IHittable> BVH;
	double VerticalFOV         = 90.0f;
	Point3 CameraPos           = Point3(0.0);
	Point3 CameraTarget        = Point3(0.0, 0.0, -1.0);
	double CameraAperture      = 0.01;
	double CameraFocusDistance = 100.0;
	std::shared_ptr<ISkyMaterial> Sky;
};
