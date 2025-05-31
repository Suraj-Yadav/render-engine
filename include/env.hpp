#pragma once

#include <Magnum/GL/Mesh.h>

#include "camera.hpp"
#include "loader.hpp"
#include "shader.hpp"

class EnvMap {
	CustomShader _shader;

	Magnum::GL::Mesh _mesh;
	ImgCubeMap _texture, _irradiance, _prefilter;
	Img2D _brdfLUT;

   public:
	EnvMap(const std::filesystem::path& path);

	void draw(Magnum::ArcBall& camera);

	auto& background() { return _texture; }
	auto& irradiance() { return _irradiance; }
	auto& prefilter() { return _prefilter; }
	auto& brdfLUT() { return _brdfLUT; }
};
