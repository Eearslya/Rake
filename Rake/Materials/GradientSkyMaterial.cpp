#include "GradientSkyMaterial.hpp"

#include "Ray.hpp"

GradientSkyMaterial::GradientSkyMaterial(const Color& a, const Color& b, double gradient)
		: AlbedoA(a), AlbedoB(b), Gradient(gradient) {}

Color GradientSkyMaterial::Sample(const Ray& ray) const {
	const float t = Gradient * (ray.Direction.y + 1.0f);
	return (1.0f - t) * AlbedoA + t * AlbedoB;
}
