#include "AABB.hpp"

AABB::AABB() : Min(std::numeric_limits<double>::max()), Max(-std::numeric_limits<double>::max()) {}

AABB::AABB(const Point3& min, const Point3& max) : Min(min), Max(max) {}

bool AABB::Hit(const Ray& ray, double tMin, double tMax) const {
	const double t1  = (Min.x - ray.Origin.x) * ray.InvDirection.x;
	const double t2  = (Max.x - ray.Origin.x) * ray.InvDirection.x;
	const double t3  = (Min.y - ray.Origin.y) * ray.InvDirection.y;
	const double t4  = (Max.y - ray.Origin.y) * ray.InvDirection.y;
	const double t5  = (Min.z - ray.Origin.z) * ray.InvDirection.z;
	const double t6  = (Max.z - ray.Origin.z) * ray.InvDirection.z;
	const double min = std::max(tMin, std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6)));
	const double max = std::min(tMax, std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6)));
	if (max < 0 || min > max) { return false; }

	return true;
}

AABB AABB::Contain(const AABB& other) const {
	return AABB(Point3(glm::min(Min.x, other.Min.x), glm::min(Min.y, other.Min.y), glm::min(Min.z, other.Min.z)),
	            Point3(glm::max(Max.x, other.Max.x), glm::max(Max.y, other.Max.y), glm::max(Max.z, other.Max.z)));
}
