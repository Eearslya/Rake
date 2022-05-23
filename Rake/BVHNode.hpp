#pragma once

#include <memory>
#include <vector>

#include "DataTypes.hpp"
#include "IHittable.hpp"

class HittableList;

class BVHNode : public IHittable {
 public:
	BVHNode();
	BVHNode(const HittableList& list);
	BVHNode(const std::vector<std::shared_ptr<IHittable>>& srcObjects, size_t start, size_t end);

	virtual bool Bounds(AABB& outBounds) const override;
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

 private:
	std::shared_ptr<IHittable> _left;
	std::shared_ptr<IHittable> _right;
	AABB _bounds;
};
