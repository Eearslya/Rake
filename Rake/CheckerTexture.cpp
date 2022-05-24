#include "CheckerTexture.hpp"

#include "SolidTexture.hpp"

CheckerTexture::CheckerTexture(const std::shared_ptr<ITexture>& odd,
                               const std::shared_ptr<ITexture>& even,
                               const glm::vec2& scale)
		: Odd(odd), Even(even), Scale(scale) {}

CheckerTexture::CheckerTexture(const Color& odd, const Color& even, const glm::vec2& scale)
		: Odd(std::make_shared<SolidTexture>(odd)), Even(std::make_shared<SolidTexture>(even)), Scale(scale) {}

Color CheckerTexture::Sample(const Point2& uv, const Point3& p) const {
	const auto sines = glm::sin(Scale.x * uv.x) * glm::sin(Scale.y * uv.y);
	return sines < 0.0 ? Odd->Sample(uv, p) : Even->Sample(uv, p);
}
