#pragma once

#include <string>
#include <vector>

#include "ITexture.hpp"

class ImageTexture : public ITexture {
 public:
	ImageTexture() = default;
	ImageTexture(const std::string& filename);

	virtual Color Sample(const Point2& uv, const Vector3& p) const override;

	glm::uvec2 Size = glm::uvec2(0);
	std::vector<Color> Pixels;
};
