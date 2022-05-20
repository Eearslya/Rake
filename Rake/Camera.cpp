#include "Camera.hpp"

#include "Random.hpp"

Camera::Camera(
	const Point3& position, const Point3& target, double vFov, double aspectRatio, double aperture, double focusDist) {
	const auto theta          = glm::radians(vFov);
	const auto h              = glm::tan(theta / 2.0);
	const auto viewportHeight = 2.0 * h;
	const auto viewportWidth  = aspectRatio * viewportHeight;

	_forward         = glm::normalize(position - target);
	_right           = glm::normalize(glm::cross(Vector3(0.0, 1.0, 0.0), _forward));
	_up              = glm::cross(_forward, _right);
	_origin          = position;
	_horizontal      = focusDist * viewportWidth * _right;
	_vertical        = focusDist * viewportHeight * _up;
	_lowerLeftCorner = _origin - _horizontal / 2.0 - _vertical / 2.0 - focusDist * _forward;
	_lensRadius      = aperture / 2.0;
}

Ray Camera::GetRay(double s, double t) const {
	const Vector3 rd     = _lensRadius * RandomInUnitDisk();
	const Vector3 offset = rd.x * _right + rd.y * _up;
	return Ray(_origin + offset, glm::normalize(_lowerLeftCorner + s * _horizontal + t * _vertical - _origin - offset));
}
