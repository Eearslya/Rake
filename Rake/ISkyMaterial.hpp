#pragma once

#include "DataTypes.hpp"

class Ray;

class ISkyMaterial {
 public:
	virtual Color Sample(const Ray& ray) const = 0;
};
