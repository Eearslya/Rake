#pragma once

#include "DataTypes.hpp"
#include "Ray.hpp"

struct HitRecord {
	Point3 Point;
	double Distance;
	Vector3 Normal;
	bool FrontFace;

	inline void SetFaceNormal(const Ray& ray, const Vector3& outwardNormal) {
		FrontFace = glm::dot(ray.Direction, outwardNormal) < 0.0;
		Normal    = FrontFace ? outwardNormal : -outwardNormal;
	}
};

class IHittable {
 public:
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const = 0;
};
