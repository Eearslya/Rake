#include "SolidSkyMaterial.hpp"

#include "Ray.hpp"
#include "SolidTexture.hpp"

SolidSkyMaterial::SolidSkyMaterial(const std::shared_ptr<ITexture>& texture) : Texture(texture) {}

SolidSkyMaterial::SolidSkyMaterial(const Color& color) : Texture(std::make_shared<SolidTexture>(color)) {}

Color SolidSkyMaterial::Sample(const Ray& ray) const {
	const auto uv =
		(Point2(glm::atan(ray.Direction.z, ray.Direction.x), glm::asin(ray.Direction.y)) * Point2(0.1591, 0.3183)) +
		Point2(0.5);
	return Texture->Sample(uv, ray.Direction);
}
