#include "MetalMaterial.hpp"

#include "IHittable.hpp"
#include "Random.hpp"
#include "Ray.hpp"

MetalMaterial::MetalMaterial(const Color& albedo, double roughness) : Albedo(albedo), Roughness(roughness) {}

Color MetalMaterial::Emit(const Point2& uv, const Point3& p) const {
	return Color(0.0);
}

bool MetalMaterial::Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const {
	const Vector3 reflected = glm::reflect(glm::normalize(ray.Direction), hit.Normal) + Roughness * RandomInUnitSphere();
	outAttenuation          = Albedo;
	outScattered            = Ray(hit.Point, glm::normalize(reflected));

	return glm::dot(outScattered.Direction, hit.Normal) > 0.0;
}
