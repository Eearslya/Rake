#include "Sphere.hpp"

#include "IMaterial.hpp"

Sphere::Sphere(const Point3& center, double radius, const std::shared_ptr<IMaterial>& material)
		: Center(center), Radius(radius), Material(material) {}

bool Sphere::Bounds(AABB& outBounds) const {
	const double r = glm::abs(Radius);
	outBounds      = AABB(Center - Vector3(r), Center + Vector3(r));

	return true;
}

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
	outRecord.Material = Material.get();
	outRecord.UV       = GetUV(outwardNormal);

	return true;
}

Point2 Sphere::GetUV(const Point3& p) const {
	const auto theta = glm::acos(-p.y);
	const auto phi   = std::atan2(-p.z, p.x) + Pi;

	return Point2(phi / (2 * Pi), theta / Pi);
}
