#pragma once

#include <memory>

#include "ITexture.hpp"

class CheckerTexture : public ITexture {
 public:
	CheckerTexture() = default;
	CheckerTexture(const std::shared_ptr<ITexture>& odd,
	               const std::shared_ptr<ITexture>& even,
	               const glm::vec2& scale = glm::vec2(10.0f));
	CheckerTexture(const Color& odd, const Color& even, const glm::vec2& scale = glm::vec2(10.0f));

	virtual Color Sample(const Point2& uv, const Point3& p) const override;

	std::shared_ptr<ITexture> Odd;
	std::shared_ptr<ITexture> Even;
	glm::vec2 Scale = glm::vec2(10.0f);
};
