#include "LambertianMaterial.hpp"

#include "IHittable.hpp"
#include "Random.hpp"
#include "SolidTexture.hpp"

LambertianMaterial::LambertianMaterial(const Color& albedo) : Texture(std::make_shared<SolidTexture>(albedo)) {}

LambertianMaterial::LambertianMaterial(const std::shared_ptr<ITexture>& texture) : Texture(texture) {}

Color LambertianMaterial::Emit(const Point2& uv, const Point3& p) const {
	return Color(0.0);
}

bool LambertianMaterial::Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const {
	Point3 target = RandomInHemisphere(hit.Normal);
	if (glm::length(target) < 0.001) { target = hit.Normal; }
	outAttenuation = Texture->Sample(hit.UV, hit.Point);
	outScattered   = Ray(hit.Point, glm::normalize(target));

	return true;
}
