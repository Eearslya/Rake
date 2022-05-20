#pragma once

#include "IHittable.hpp"

class Sphere : public IHittable {
 public:
	Sphere() = default;
	Sphere(const Point3& center, double radius);

	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

	Point3 Center;
	double Radius;
};
