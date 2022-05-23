#pragma once

#include "IMaterial.hpp"

class MetalMaterial : public IMaterial {
 public:
	MetalMaterial(const Color& albedo, double roughness);

	virtual Color Emit(const Point2& uv, const Point3& p) const override;
	virtual bool Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const override;

	Color Albedo;
	double Roughness;
};
