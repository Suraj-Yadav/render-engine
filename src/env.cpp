#include "env.hpp"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Sampler.h>
#include <Magnum/Shaders/GenericGL.h>

#include <array>

static constexpr auto COORDS = std::to_array({
	Magnum::Vector3{-1.0f, 3.0f, 0.9999f},
	Magnum::Vector3{-1.0f, -1.0f, 0.9999f},
	Magnum::Vector3{3.0f, -1.0f, 0.9999f},
});

EnvMap::EnvMap(const std::filesystem::path& path)
	: _shader("./shaders/background.vert", "./shaders/background.frag") {
	_mesh.setCount(COORDS.size())
		.addVertexBuffer(
			Magnum::GL::Buffer{COORDS}, 0,
			Magnum::Shaders::GenericGL3D::Position{});

	auto input = loadImage(path);

	_texture.format = Magnum::PixelFormat::RGB16F;
	_texture.size = Magnum::Vector2i(input.size.min());
	_texture.texture
		.setStorage(
			1, Magnum::GL::textureFormat(_texture.format), _texture.size)
		.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(
			Magnum::SamplerFilter::Linear, Magnum::SamplerMipmap::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	generateCubeMap(
		_texture, 1, "./shaders/equirectangular_to_cubemap_copy.frag",
		[&](CustomShader& shader, int) {
			shader.setTexture("u_textureData", input, 0);
		});
	_texture.texture.generateMipmap();

	_irradiance.format = Magnum::PixelFormat::RGB16F;
	_irradiance.size = Magnum::Vector2i(32);
	_irradiance.texture
		.setStorage(
			1, Magnum::GL::textureFormat(_irradiance.format), _irradiance.size)
		.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(Magnum::SamplerFilter::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	generateCubeMap(
		_irradiance, 1, "./shaders/irradiance_convolution.frag",
		[&](CustomShader& shader, int) {
			shader.setTexture("u_textureData", _texture, 0);
		});

	constexpr auto maxMipLevels = 5;
	_prefilter.format = Magnum::PixelFormat::RGB16F;
	_prefilter.size = Magnum::Vector2i(128);
	_prefilter.texture
		.setStorage(
			maxMipLevels, Magnum::GL::textureFormat(_prefilter.format),
			_prefilter.size)
		.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(
			Magnum::SamplerFilter::Linear, Magnum::SamplerMipmap::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear)
		.generateMipmap();

	generateCubeMap(
		_prefilter, maxMipLevels, "./shaders/prefilter.frag",
		[&](CustomShader& shader, int mip) {
			auto roughness = (float)mip / (maxMipLevels - 1);
			shader.setTexture("u_textureData", _texture, 0)
				.setUniformT("u_roughness", roughness);
		});

	_brdfLUT.format = Magnum::PixelFormat::RG16F;
	_brdfLUT.size = Magnum::Vector2i(512);
	_brdfLUT.texture
		.setStorage(
			maxMipLevels, Magnum::GL::textureFormat(_brdfLUT.format),
			_brdfLUT.size)
		.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(Magnum::SamplerFilter::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear)
		.generateMipmap();
	generateImg2D(
		_brdfLUT, 1, "./shaders/brdf.frag", [&](CustomShader& shader, int) {});
}

void EnvMap::draw(Magnum::ArcBall& camera) {
	Magnum::GL::Renderer::setDepthMask(GL_FALSE);
	_shader.setTexture("u_envCubeMap", _texture, 0)
		.setUniformT(
			"u_viewProjection", camera.projectionMatrix() * camera.viewMatrix())
		.draw(_mesh);
	Magnum::GL::Renderer::setDepthMask(GL_TRUE);
}
