#include "ImageTexture.hpp"

#include <stb_image.h>

#include <Luna/Utility/Log.hpp>

using Luna::Log;

ImageTexture::ImageTexture(const std::string& filename) {
	if (stbi_is_hdr(filename.c_str())) {
		int x, y, comp;
		float* pixels = stbi_loadf(filename.c_str(), &x, &y, &comp, 3);
		if (!pixels) {
			Log::Error("ImageTexture", "Failed to open texture file: {}", filename);
			return;
		}

		Size                  = glm::uvec2(x, y);
		const auto pixelCount = x * y;
		const auto floatCount = pixelCount * 3;
		Pixels.resize(pixelCount);

		size_t pixel = 0;
		for (uint64_t p = 0; p < floatCount; p += 3) { Pixels[pixel++] = Color(pixels[p], pixels[p + 1], pixels[p + 2]); }

		stbi_image_free(pixels);
	} else {
		int x, y, comp;
		stbi_uc* pixels = stbi_load(filename.c_str(), &x, &y, &comp, 3);
		if (!pixels) {
			Log::Error("ImageTexture", "Failed to open texture file: {}", filename);
			return;
		}

		Size                  = glm::uvec2(x, y);
		const auto pixelCount = x * y;
		const auto byteCount  = pixelCount * 3;
		Pixels.resize(pixelCount);
		size_t pixel = 0;
		for (uint64_t p = 0; p < byteCount; p += 3) {
			const uint8_t r = pixels[p] & 0xff;
			const uint8_t g = pixels[p + 1] & 0xff;
			const uint8_t b = pixels[p + 2] & 0xff;
			Pixels[pixel++] =
				Color(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f);
		}

		stbi_image_free(pixels);
	}
}

Color ImageTexture::Sample(const Point2& uv, const Vector3& p) const {
	if (Pixels.size() == 0) { return Color(0, 1, 1); }

	const auto u = glm::clamp(uv.x, 0.0, 1.0);
	const auto v = 1.0 - glm::clamp(uv.y, 0.0, 1.0);
	const auto x = glm::clamp(static_cast<unsigned int>(u * Size.x), 0u, Size.x - 1);
	const auto y = glm::clamp(static_cast<unsigned int>(v * Size.y), 0u, Size.y - 1);

	return Pixels[(y * Size.x) + x];
}
