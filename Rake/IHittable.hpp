#pragma once

#include <memory>

#include "AABB.hpp"
#include "DataTypes.hpp"
#include "Ray.hpp"

class IMaterial;

struct HitRecord {
	Point3 Point;
	double Distance;
	Vector3 Normal;
	bool FrontFace;
	Point2 UV;
	const IMaterial* Material = nullptr;

	inline void SetFaceNormal(const Ray& ray, const Vector3& outwardNormal) {
		FrontFace = glm::dot(ray.Direction, outwardNormal) < 0.0;
		Normal    = FrontFace ? outwardNormal : -outwardNormal;
	}
};

class IHittable {
 public:
	virtual bool Bounds(AABB& outBounds) const                                             = 0;
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const = 0;
};
