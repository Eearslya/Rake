#pragma once

#include "ISkyMaterial.hpp"

class GradientSkyMaterial : public ISkyMaterial {
 public:
	GradientSkyMaterial(const Color& a, const Color& b, double gradient);

	virtual Color Sample(const Ray& ray) const override;

	Color AlbedoA;
	Color AlbedoB;
	double Gradient;
};
