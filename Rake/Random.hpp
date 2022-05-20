#pragma once

#include <cmath>
#include <random>

#include "DataTypes.hpp"

inline double RandomDouble() {
	static std::uniform_real_distribution<double> distribution(0.0, 1.0);
	static std::mt19937 generator;
	return distribution(generator);
}

inline double RandomDouble(double min, double max) {
	return min + (max - min) * RandomDouble();
}

inline Vector3 RandomInUnitSphere() {
	const auto u        = RandomDouble();
	const auto v        = RandomDouble();
	const auto theta    = u * 2.0 * Pi;
	const auto phi      = glm::acos(2.0 * v - 1.0);
	const auto r        = std::cbrt(RandomDouble());
	const auto sinTheta = glm::sin(theta);
	const auto cosTheta = glm::cos(theta);
	const auto sinPhi   = glm::sin(phi);
	const auto cosPhi   = glm::cos(phi);
	const auto x        = r * sinPhi * cosTheta;
	const auto y        = r * sinPhi * sinTheta;
	const auto z        = r * cosPhi;
	return Vector3(x, y, z);
}

inline Vector3 RandomInHemisphere(const Vector3& normal) {
	const Vector3 inUnitSphere = RandomInUnitSphere();
	return (glm::dot(inUnitSphere, normal) > 0.0) ? inUnitSphere : -inUnitSphere;
}

inline Vector3 RandomUnitVector() {
	return glm::normalize(RandomInUnitSphere());
}

inline Vector3 RandomInUnitDisk() {
	const double r     = glm::sqrt(RandomDouble(0.0, 1.0));
	const double theta = RandomDouble(0.0, 1.0) * 2.0 * Pi;
	return Vector3(r * glm::cos(theta), r * glm::sin(theta), 0.0);
}
