#include "DielectricMaterial.hpp"

#include "IHittable.hpp"
#include "Random.hpp"

DielectricMaterial::DielectricMaterial(double index) : IndexOfRefraction(index) {}

Color DielectricMaterial::Emit(const Point2& uv, const Point3& p) const {
	return Color(0.0);
}

bool DielectricMaterial::Scatter(const Ray& ray, const HitRecord& hit, Color& outAttenuation, Ray& outScattered) const {
	const double refractionRatio = hit.FrontFace ? (1.0 / IndexOfRefraction) : IndexOfRefraction;
	const auto cosTheta          = glm::min(glm::dot(-ray.Direction, hit.Normal), 1.0);
	const auto sinTheta          = glm::sqrt(1.0 - cosTheta * cosTheta);
	const bool cannotRefract     = refractionRatio * sinTheta > 1.0;
	const bool reflect           = cannotRefract || Reflectance(cosTheta, refractionRatio) > RandomDouble();
	const Vector3 refracted =
		reflect ? glm::reflect(ray.Direction, hit.Normal) : glm::refract(ray.Direction, hit.Normal, refractionRatio);

	outAttenuation = Color(1.0);
	outScattered   = Ray(hit.Point, glm::normalize(refracted));

	return true;
}

double DielectricMaterial::Reflectance(double cosine, double refIndex) {
	auto r0 = (1.0 - refIndex) / (1.0 + refIndex);
	r0      = r0 * r0;
	return r0 + (1.0 - r0) * glm::pow(1.0 - cosine, 5.0);
}
