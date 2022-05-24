#include "DiffuseLightMaterial.hpp"

#include "SolidTexture.hpp"

DiffuseLightMaterial::DiffuseLightMaterial(const std::shared_ptr<ITexture>& texture) : Texture(texture) {}

DiffuseLightMaterial::DiffuseLightMaterial(const Color& color) : Texture(std::make_shared<SolidTexture>(color)) {}

Color DiffuseLightMaterial::Emit(const Point2& uv, const Point3& p) const {
	return Texture->Sample(uv, p);
}

bool DiffuseLightMaterial::Scatter(const Ray& ray,
                                   const HitRecord& hit,
                                   Color& outAttenuation,
                                   Ray& outScattered) const {
	return false;
}
