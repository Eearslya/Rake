#pragma once

#include "DataTypes.hpp"
#include "Ray.hpp"

class AABB {
 public:
	AABB();
	AABB(const Point3& min, const Point3& max);

	bool Hit(const Ray& ray, double tMin, double tMax) const;
	AABB Contain(const AABB& other) const;

	Point3 Min;
	Point3 Max;
};
