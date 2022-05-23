#include "SolidSkyMaterial.hpp"

#include "SolidTexture.hpp"

SolidSkyMaterial::SolidSkyMaterial(const std::shared_ptr<ITexture>& texture) : Texture(texture) {}

SolidSkyMaterial::SolidSkyMaterial(const Color& color) : Texture(std::make_shared<SolidTexture>(color)) {}

Color SolidSkyMaterial::Sample(const Ray& ray) const {}
