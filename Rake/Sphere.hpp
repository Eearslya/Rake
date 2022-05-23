#pragma once

#include "IHittable.hpp"

class IMaterial;

class Sphere : public IHittable {
 public:
	Sphere() = default;
	Sphere(const Point3& center, double radius, const std::shared_ptr<IMaterial>& material);

	virtual bool Bounds(AABB& outBounds) const override;
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

	Point3 Center;
	double Radius;
	std::shared_ptr<IMaterial> Material;

 private:
	Point2 GetUV(const Point3& p) const;
};
