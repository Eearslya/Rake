#include "AABB.hpp"

AABB::AABB() : Min(std::numeric_limits<double>::max()), Max(-std::numeric_limits<double>::max()) {}

AABB::AABB(const Point3& min, const Point3& max) : Min(min), Max(max) {}

bool AABB::Hit(const Ray& ray, double tMin, double tMax) const {
	for (int a = 0; a < 3; ++a) {
		const auto invD = 1.0 / ray.Direction[a];
		auto t0         = (Min[a] - ray.Origin[a]) * invD;
		auto t1         = (Max[a] - ray.Origin[a]) * invD;
		if (invD < 0.0) { std::swap(t0, t1); }
		tMin = t0 > tMin ? t0 : tMin;
		tMax = t1 < tMax ? t1 : tMax;
		if (tMax <= tMin) { return false; }
	}

	return true;
}

AABB AABB::Contain(const AABB& other) const {
	return AABB(Point3(glm::min(Min.x, other.Min.x), glm::min(Min.y, other.Min.y), glm::min(Min.z, other.Min.z)),
	            Point3(glm::max(Max.x, other.Max.x), glm::max(Max.y, other.Max.y), glm::max(Max.z, other.Max.z)));
}
