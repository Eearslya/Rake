#pragma once

#include <memory>

#include "ITexture.hpp"

class CheckerTexture : public ITexture {
 public:
	CheckerTexture() = default;
	CheckerTexture(const std::shared_ptr<ITexture>& odd, const std::shared_ptr<ITexture>& even, double scale = 10.0);
	CheckerTexture(const Color& odd, const Color& even, double scale = 10.0);

	virtual Color Sample(const Point2& uv, const Point3& p) const override;

	std::shared_ptr<ITexture> Odd;
	std::shared_ptr<ITexture> Even;
	double Scale = 10.0;
};
