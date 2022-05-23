#pragma once

#include "DataTypes.hpp"

class ITexture {
 public:
	virtual Color Sample(const Point2& uv, const Point3& p) const = 0;
};
