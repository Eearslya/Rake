#include "HittableList.hpp"

HittableList::HittableList(const std::shared_ptr<IHittable>& object) {
	Add(object);
}

void HittableList::Add(const std::shared_ptr<IHittable>& object) {
	Objects.push_back(object);
}

void HittableList::Clear() {
	Objects.clear();
}

bool HittableList::Bounds(AABB& outBounds) const {
	if (Objects.empty()) { return false; }

	AABB bounds;
	for (const auto& obj : Objects) {
		if (!obj->Bounds(bounds)) { return false; }
		outBounds = outBounds.Contain(bounds);
	}

	return true;
}

bool HittableList::Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const {
	HitRecord hit;
	bool hitAnything  = false;
	double closestHit = tMax;

	for (const auto& object : Objects) {
		if (object->Hit(ray, tMin, closestHit, hit)) {
			hitAnything = true;
			closestHit  = hit.Distance;
			outRecord   = hit;
		}
	}

	return hitAnything;
}
