#pragma once

#include "DataTypes.hpp"

struct HitRecord;
class Ray;

class IMaterial {
 public:
	virtual Color Emit(const Point2& uv, const Point3& p) const                                                = 0;
	virtual bool Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const = 0;
};
