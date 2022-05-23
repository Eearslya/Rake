#pragma once

#include "ITexture.hpp"

class SolidTexture : public ITexture {
 public:
	SolidTexture() = default;
	SolidTexture(const Color& c);
	SolidTexture(float r, float g, float b);

	virtual Color Sample(const Point2& uv, const Vector3& p) const override;

	Color Albedo;
};
