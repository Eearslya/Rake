#pragma once

#include <memory>

#include "ISkyMaterial.hpp"
#include "ITexture.hpp"

class SolidSkyMaterial : public ISkyMaterial {
 public:
	SolidSkyMaterial() = default;
	SolidSkyMaterial(const std::shared_ptr<ITexture>& texture);
	SolidSkyMaterial(const Color& color);

	virtual Color Sample(const Ray& ray) const override;

	std::shared_ptr<ITexture> Texture;
};
