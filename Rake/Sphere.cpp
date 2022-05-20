#include "Sphere.hpp"

Sphere::Sphere(const Point3& center, double radius) : Center(center), Radius(radius) {}

bool Sphere::Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const {
	const Vector3 oc = ray.Origin - Center;
	const auto halfB = glm::dot(oc, ray.Direction);
	const auto c     = glm::dot(oc, oc) - Radius * Radius;

	const auto discriminant = halfB * halfB - c;
	if (discriminant < 0.0) { return false; }

	const auto sqrtd = glm::sqrt(discriminant);
	double root      = -halfB - sqrtd;
	if (root < tMin || tMax < root) {
		root = -halfB + sqrtd;
		if (root < tMin || tMax < root) { return false; }
	}

	outRecord.Distance          = root;
	outRecord.Point             = ray.At(outRecord.Distance);
	const Vector3 outwardNormal = (outRecord.Point - Center) / Radius;
	outRecord.SetFaceNormal(ray, outwardNormal);

	return true;
}
