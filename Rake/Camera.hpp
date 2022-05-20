#pragma once

#include "DataTypes.hpp"
#include "Ray.hpp"

class Camera {
 public:
	Camera() = default;
	Camera(
		const Point3& position, const Point3& target, double vFov, double aspectRatio, double aperture, double focusDist);

	Ray GetRay(double s, double t) const;

 private:
	Point3 _origin;
	Point3 _lowerLeftCorner;
	Vector3 _horizontal;
	Vector3 _vertical;
	Vector3 _forward;
	Vector3 _right;
	Vector3 _up;
	double _lensRadius;
};
