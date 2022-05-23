#pragma once

#include <memory>

#include "IHittable.hpp"
#include "IMaterial.hpp"

class XYRectangle : public IHittable {
 public:
	XYRectangle() = default;
	XYRectangle(const Point2& min, const Point2& max, double z, const std::shared_ptr<IMaterial>& material);

	virtual bool Bounds(AABB& outBounds) const override;
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

	Point2 Min;
	Point2 Max;
	double Z = 0.0;
	std::shared_ptr<IMaterial> Material;
};

class XZRectangle : public IHittable {
 public:
	XZRectangle() = default;
	XZRectangle(const Point2& min, const Point2& max, double y, const std::shared_ptr<IMaterial>& material);

	virtual bool Bounds(AABB& outBounds) const override;
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

	Point2 Min;
	Point2 Max;
	double Y = 0.0;
	std::shared_ptr<IMaterial> Material;
};

class YZRectangle : public IHittable {
 public:
	YZRectangle() = default;
	YZRectangle(const Point2& min, const Point2& max, double x, const std::shared_ptr<IMaterial>& material);

	virtual bool Bounds(AABB& outBounds) const override;
	virtual bool Hit(const Ray& ray, double tMin, double tMax, HitRecord& outRecord) const override;

	Point2 Min;
	Point2 Max;
	double X = 0.0;
	std::shared_ptr<IMaterial> Material;
};
