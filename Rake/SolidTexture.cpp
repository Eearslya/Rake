#include "SolidTexture.hpp"

SolidTexture::SolidTexture(const Color& c) : Albedo(c) {}

SolidTexture::SolidTexture(float r, float g, float b) : Albedo(Color(r, g, b)) {}

Color SolidTexture::Sample(const Point2& uv, const Vector3& p) const {
	return Albedo;
}
