#pragma once

#include <memory>

#include "IMaterial.hpp"
#include "ITexture.hpp"

class DiffuseLightMaterial : public IMaterial {
 public:
	DiffuseLightMaterial(const std::shared_ptr<ITexture>& texture);
	DiffuseLightMaterial(const Color& color);

	virtual Color Emit(const Point2& uv, const Point3& p) const override;
	virtual bool Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const override;

	std::shared_ptr<ITexture> Texture;
};
