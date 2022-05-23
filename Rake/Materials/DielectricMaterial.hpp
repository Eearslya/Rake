#pragma once

#include "IMaterial.hpp"

class DielectricMaterial : public IMaterial {
 public:
	DielectricMaterial(double index);

	virtual Color Emit(const Point2& uv, const Point3& p) const override;
	virtual bool Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const override;

	double IndexOfRefraction;

 private:
	static double Reflectance(double cosine, double refIndex);
};
