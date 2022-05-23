#pragma once

#include "Rectangle.hpp"

class XYPlane : public XYRectangle {
 public:
	XYPlane() = default;
	XYPlane(double z, const std::shared_ptr<IMaterial>& material);
};

class XZPlane : public XZRectangle {
 public:
	XZPlane() = default;
	XZPlane(double y, const std::shared_ptr<IMaterial>& material);
};

class YZPlane : public YZRectangle {
 public:
	YZPlane() = default;
	YZPlane(double x, const std::shared_ptr<IMaterial>& material);
};
