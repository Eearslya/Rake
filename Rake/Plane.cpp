#include "Plane.hpp"

XYPlane::XYPlane(double z, const std::shared_ptr<IMaterial>& material)
		: XYRectangle(Point2(-std::numeric_limits<double>::infinity()),
                  Point2(std::numeric_limits<double>::infinity()),
                  z,
                  material) {}

XZPlane::XZPlane(double y, const std::shared_ptr<IMaterial>& material)
		: XZRectangle(
				Point2(-std::numeric_limits<double>::max()), Point2(std::numeric_limits<double>::max()), y, material) {}

YZPlane::YZPlane(double x, const std::shared_ptr<IMaterial>& material)
		: YZRectangle(Point2(-std::numeric_limits<double>::infinity()),
                  Point2(std::numeric_limits<double>::infinity()),
                  x,
                  material) {}
