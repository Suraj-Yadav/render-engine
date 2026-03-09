#pragma once

#include <Magnum/GL/Mesh.h>

#include "camera.hpp"
#include "image.hpp"
#include "shader.hpp"

class EnvMap {
	CustomShader _shader;

	Magnum::GL::Mesh _mesh;
	ImgCubeMap cubemapTexture, lambertianTexture, ggxTexture, sheenTexture;
	Img2D ggxLutTexture, charlieLutTexture, sheenELut;

   public:
	EnvMap();
	void update(std::filesystem::path path);

	void draw(Magnum::ArcBall& camera);

	auto& diffuse() { return lambertianTexture; }
	auto& specular() { return ggxTexture; }
	auto& sheen() { return sheenTexture; }
	auto& lut() { return ggxLutTexture; }
	auto& sheenLUT() { return charlieLutTexture; }
	auto& sheenELUT() { return sheenELut; }
};
