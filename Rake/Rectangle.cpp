#include "Rectangle.hpp"

static Point3 GetPrimaryDir(const Vector3& normal) {
	const Vector3 a     = glm::cross(normal, Vector3(1, 0, 0));
	const Vector3 b     = glm::cross(normal, Vector3(0, 1, 0));
	const Vector3 maxAB = glm::dot(a, a) < glm::dot(b, b) ? b : a;
	const Vector3 c     = glm::cross(normal, Vector3(0, 0, 1));
	return glm::normalize(glm::dot(maxAB, maxAB) < glm::dot(c, c) ? c : maxAB);
}

XYRectangle::XYRectangle(const Point2& min, const Point2& max, double z, const std::shared_ptr<IMaterial>& material)
		: Min(min), Max(max), Z(z), Material(material) {}

bool XYRectangle::Bounds(AABB& outBounds) const {
	outBounds = AABB(Point3(Min.x, Min.y, Z - 0.0001), Point3(Max.x, Max.y, Z + 0.0001));

	return true;
}

bool XYRectangle::Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const {
	const auto t = (Z - ray.Origin.z) / ray.Direction.z;
	if (t < tMin || t > tMax) { return false; }

	const auto x = ray.Origin.x + t * ray.Direction.x;
	const auto y = ray.Origin.y + t * ray.Direction.y;
	if (x < Min.x || x > Max.x || y < Min.y || y > Max.y) { return false; }

	outRecord.Distance       = t;
	outRecord.Point          = ray.At(outRecord.Distance);
	const auto outwardNormal = Point3(0, 0, 1);
	outRecord.SetFaceNormal(ray, outwardNormal);
	outRecord.Material = Material.get();
	const auto u       = GetPrimaryDir(outwardNormal);
	const auto v       = glm::cross(outwardNormal, u);
	outRecord.UV       = Point2(glm::dot(u, outRecord.Point), glm::dot(v, outRecord.Point));

	return true;
}

XZRectangle::XZRectangle(const Point2& min, const Point2& max, double y, const std::shared_ptr<IMaterial>& material)
		: Min(min), Max(max), Y(y), Material(material) {}

bool XZRectangle::Bounds(AABB& outBounds) const {
	outBounds = AABB(Point3(Min.x, Y - 0.0001, Min.y), Point3(Max.x, Y + 0.0001, Max.y));

	return true;
}

bool XZRectangle::Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const {
	const auto t = (Y - ray.Origin.y) / ray.Direction.y;
	if (t < tMin || t > tMax) { return false; }

	const auto x = ray.Origin.x + t * ray.Direction.x;
	const auto z = ray.Origin.z + t * ray.Direction.z;
	if (x < Min.x || x > Max.x || z < Min.y || z > Max.y) { return false; }

	outRecord.Distance       = t;
	outRecord.Point          = ray.At(outRecord.Distance);
	const auto outwardNormal = Point3(0, 1, 0);
	outRecord.SetFaceNormal(ray, outwardNormal);
	outRecord.Material = Material.get();
	const auto u       = GetPrimaryDir(outwardNormal);
	const auto v       = glm::cross(outwardNormal, u);
	outRecord.UV       = Point2(glm::dot(u, outRecord.Point), glm::dot(v, outRecord.Point));

	return true;
}

YZRectangle::YZRectangle(const Point2& min, const Point2& max, double x, const std::shared_ptr<IMaterial>& material)
		: Min(min), Max(max), X(x), Material(material) {}

bool YZRectangle::Bounds(AABB& outBounds) const {
	outBounds = AABB(Point3(X - 0.0001, Min.x, Min.y), Point3(X + 0.0001, Max.x, Max.y));

	return true;
}

bool YZRectangle::Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const {
	const auto t = (X - ray.Origin.x) / ray.Direction.x;
	if (t < tMin || t > tMax) { return false; }

	const auto y = ray.Origin.y + t * ray.Direction.y;
	const auto z = ray.Origin.z + t * ray.Direction.z;
	if (y < Min.x || y > Max.x || z < Min.y || z > Max.y) { return false; }

	outRecord.Distance       = t;
	outRecord.Point          = ray.At(outRecord.Distance);
	const auto outwardNormal = Point3(1, 0, 0);
	outRecord.SetFaceNormal(ray, outwardNormal);
	outRecord.Material = Material.get();
	const auto u       = GetPrimaryDir(outwardNormal);
	const auto v       = glm::cross(outwardNormal, u);
	outRecord.UV       = Point2(glm::dot(u, outRecord.Point), glm::dot(v, outRecord.Point));

	return true;
}
