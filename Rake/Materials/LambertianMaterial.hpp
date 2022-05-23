#pragma once

#include <memory>

#include "IMaterial.hpp"
#include "ITexture.hpp"

class LambertianMaterial : public IMaterial {
 public:
	LambertianMaterial(const Color& albedo);
	LambertianMaterial(const std::shared_ptr<ITexture>& texture);

	virtual Color Emit(const Point2& uv, const Point3& p) const override;
	virtual bool Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const override;

	std::shared_ptr<ITexture> Texture;
};
