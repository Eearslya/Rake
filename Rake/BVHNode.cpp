#include "BVHNode.hpp"

#include <Luna/Utility/Log.hpp>
#include <stdexcept>

#include "HittableList.hpp"
#include "Random.hpp"

using Luna::Log;

static inline bool BoxCompare(const std::shared_ptr<IHittable>& a, const std::shared_ptr<IHittable>& b, int axis) {
	AABB boxA, boxB;
	if (!a->Bounds(boxA) || !b->Bounds(boxB)) { throw std::runtime_error("Failed to get AABB bounds!"); }

	return boxA.Min[axis] < boxB.Min[axis];
}

static inline bool BoxXCompare(const std::shared_ptr<IHittable>& a, const std::shared_ptr<IHittable>& b) {
	return BoxCompare(a, b, 0);
}

static inline bool BoxYCompare(const std::shared_ptr<IHittable>& a, const std::shared_ptr<IHittable>& b) {
	return BoxCompare(a, b, 1);
}

static inline bool BoxZCompare(const std::shared_ptr<IHittable>& a, const std::shared_ptr<IHittable>& b) {
	return BoxCompare(a, b, 2);
}

BVHNode::BVHNode() {}

BVHNode::BVHNode(const HittableList& list) : BVHNode(list.Objects, 0, list.Objects.size()) {}

BVHNode::BVHNode(const std::vector<std::shared_ptr<IHittable>>& srcObjects, size_t start, size_t end) {
	auto objects            = srcObjects;
	int axis                = RandomInt(0, 2);
	auto comparator         = (axis == 0) ? BoxXCompare : (axis == 1) ? BoxYCompare : BoxZCompare;
	const size_t objectSpan = end - start;

	if (objectSpan == 0) { throw std::runtime_error("Cannot construct a BVH with 0 objects!"); }

	if (objectSpan == 1) {
		_left = _right = objects[start];
	} else if (objectSpan == 2) {
		if (comparator(objects[start], objects[start + 1])) {
			_left  = objects[start];
			_right = objects[start + 1];
		} else {
			_left  = objects[start + 1];
			_right = objects[start];
		}
	} else {
		std::sort(objects.begin() + start, objects.begin() + end, comparator);

		const auto mid = start + objectSpan / 2;
		_left          = std::make_shared<BVHNode>(objects, start, mid);
		_right         = std::make_shared<BVHNode>(objects, mid, end);
	}

	AABB boxLeft, boxRight;
	if (!_left->Bounds(boxLeft) || !_right->Bounds(boxRight)) {
		throw std::runtime_error("Failed to calculate bounds for BVHNode!");
	}

	_bounds = boxLeft.Contain(boxRight);
}

bool BVHNode::Bounds(AABB& outBounds) const {
	outBounds = _bounds;

	return true;
}

bool BVHNode::Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const {
	if (!_bounds.Hit(ray, tMin, tMax)) { return false; }

	const bool hitLeft  = _left->Hit(ray, tMin, tMax, outRecord);
	const bool hitRight = _right->Hit(ray, tMin, hitLeft ? outRecord.Distance : tMax, outRecord);

	return hitLeft || hitRight;
}
