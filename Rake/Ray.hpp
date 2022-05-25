#pragma once

#include "DataTypes.hpp"

class Ray {
 public:
	Ray() = default;
	Ray(const Point3& origin, const Vector3& direction)
			: Origin(origin), Direction(direction), InvDirection(1.0 / Direction) {}

	Point3 At(double t) const {
		return Origin + t * Direction;
	}

	Point3 Origin        = Point3(0.0);
	Vector3 Direction    = Vector3(0.0);
	Vector3 InvDirection = Vector3(0.0);
};
