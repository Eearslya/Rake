#pragma once

#include <memory>
#include <vector>

#include "IHittable.hpp"

class HittableList : public IHittable {
 public:
	HittableList() = default;
	HittableList(const std::shared_ptr<IHittable>& object);

	void Add(const std::shared_ptr<IHittable>& object);
	void Clear();

	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

	template <typename T, typename... Args>
	void Add(Args&&... args) {
		Add(std::make_shared<T>(std::forward<Args>(args)...));
	}

	std::vector<std::shared_ptr<IHittable>> Objects;
};
