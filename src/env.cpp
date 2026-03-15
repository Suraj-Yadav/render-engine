#include "env.hpp"

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Sampler.h>
#include <Magnum/Shaders/GenericGL.h>

#include <array>
#include <filesystem>

#include "image.hpp"
#include "logging.hpp"

static constexpr auto COORDS = std::to_array({
	Magnum::Vector3{-1.0f, 3.0f, 0.9999f},
	Magnum::Vector3{-1.0f, -1.0f, 0.9999f},
	Magnum::Vector3{3.0f, -1.0f, 0.9999f},
});

EnvMap::EnvMap()
	: _shader(
		  PARENT / "../shaders/background.vert",
		  PARENT / "../shaders/background.frag") {
	_mesh.setCount(COORDS.size())
		.addVertexBuffer(
			Magnum::GL::Buffer{COORDS}, 0,
			Magnum::Shaders::GenericGL3D::Position{});

	constexpr auto LUT_RESOLUTION = 1024;
	constexpr auto SAMPLE_COUNT = 512;
	constexpr auto FMT = Magnum::PixelFormat::RGBA16F;

	cache(
		ggxLutTexture,
		std::filesystem::temp_directory_path() / "ggxLutTexture.bin", [&]() {
			ggxLutTexture = {LUT_RESOLUTION, FMT};
			ggxLutTexture.setStorage();
			generateImg2D(
				ggxLutTexture, 1, PARENT / "../shaders/ibl_filtering.frag",
				[&](CustomShader& shader, int) {
					shader.setUniformT("u_roughness", 0.0f);
					shader.setUniformT("u_sampleCount", SAMPLE_COUNT);
					shader.setUniformT("u_width", 0);
					shader.setUniformT("u_lodBias", 0.0f);
					shader.setUniformT("u_distribution", 1);
					shader.setUniformT("u_isGeneratingLUT", 1);
				});
		});

	ggxLutTexture.texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(Magnum::SamplerFilter::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	cache(
		charlieLutTexture,
		std::filesystem::temp_directory_path() / "charlieLutTexture.bin",
		[&]() {
			charlieLutTexture = {LUT_RESOLUTION, FMT};
			charlieLutTexture.setStorage();
			generateImg2D(
				charlieLutTexture, 1, PARENT / "../shaders/ibl_filtering.frag",
				[&](CustomShader& shader, int) {
					shader.setUniformT("u_roughness", 0.0f);
					shader.setUniformT("u_sampleCount", SAMPLE_COUNT);
					shader.setUniformT("u_width", 0);
					shader.setUniformT("u_lodBias", 0.0f);
					shader.setUniformT("u_distribution", 2);
					shader.setUniformT("u_isGeneratingLUT", 1);
				});
		});

	charlieLutTexture.texture
		.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(Magnum::SamplerFilter::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	sheenELut = loadImage(PARENT / "../images/lut_sheen_E.png");
}

void EnvMap::update(std::filesystem::path path) {
	path = std::filesystem::canonical(path);

	auto input = loadImage(path);

	const auto SIZE = std::min(1024, input.size.min());
	constexpr auto FMT = Magnum::PixelFormat::RGBA16F;

	auto BASE_PATH = (std::filesystem::temp_directory_path() /
					  fmt::format("{:X}-", std::filesystem::hash_value(path)))
						 .string();

	cache(cubemapTexture, BASE_PATH + "cubemapTexture.bin", [&]() {
		//
		cubemapTexture = {SIZE, FMT};

		cubemapTexture.setStorage();

		generateCubeMap(
			cubemapTexture, 1,
			PARENT / "../shaders/equirectangular_to_cubemap_copy.frag",
			[&](CustomShader& shader, int) {
				shader.setTexture("u_textureData", input, 0);
			});
		cubemapTexture.texture.generateMipmap();
	});
	cubemapTexture.texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(
			Magnum::SamplerFilter::Linear, Magnum::SamplerMipmap::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	constexpr auto LAMBERTIAN_SAMPLE_COUNT = 2048;

	cache(lambertianTexture, BASE_PATH + "lambertianTexture.bin", [&]() {
		lambertianTexture = {SIZE, FMT};
		lambertianTexture.setStorage();

		generateCubeMap(
			lambertianTexture, 1, PARENT / "../shaders/ibl_filtering.frag",
			[&](CustomShader& shader, int) {
				shader.setTexture("u_cubemapTexture", cubemapTexture, 0);
				shader.setUniformT("u_roughness", 0.0f);
				shader.setUniformT("u_sampleCount", LAMBERTIAN_SAMPLE_COUNT);
				shader.setUniformT("u_width", SIZE);
				shader.setUniformT("u_lodBias", 0.0f);
				shader.setUniformT("u_distribution", 0);
				shader.setUniformT("u_isGeneratingLUT", 0);
				shader.setUniformT("u_floatTexture", 1);
				shader.setUniformT("u_intensityScale", 1.0f);
			});
	});
	lambertianTexture.texture
		.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(Magnum::SamplerFilter::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	const auto MAX_MIP_LEVELS = int(Magnum::Math::log2(SIZE) + 1 - 4);

	cache(ggxTexture, BASE_PATH + "ggxTexture.bin", [&]() {
		ggxTexture = {SIZE, FMT, MAX_MIP_LEVELS + 1};
		ggxTexture.setStorage();

		constexpr auto GGX_SAMPLE_COUNT = 2048;
		generateCubeMap(
			ggxTexture, MAX_MIP_LEVELS + 1,
			PARENT / "../shaders/ibl_filtering.frag",
			[&](CustomShader& shader, int mip) {
				auto roughness = (float)mip / (MAX_MIP_LEVELS - 1);
				shader.setUniformT("u_distribution", 1);
				shader.setUniformT("u_roughness", roughness);
				shader.setTexture("u_cubemapTexture", cubemapTexture, 0);
				shader.setUniformT("u_sampleCount", GGX_SAMPLE_COUNT);
				shader.setUniformT("u_width", SIZE);
				shader.setUniformT("u_lodBias", 0.0f);
				shader.setUniformT("u_isGeneratingLUT", 0);
				shader.setUniformT("u_floatTexture", 1);
				shader.setUniformT("u_intensityScale", 1.0f);
			});
	});

	ggxTexture.texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(
			Magnum::SamplerFilter::Linear, Magnum::SamplerMipmap::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);

	cache(sheenTexture, BASE_PATH + "sheenTexture.bin", [&]() {
		sheenTexture = {SIZE, FMT, MAX_MIP_LEVELS + 1};
		sheenTexture.setStorage();

		constexpr auto SHEEN_SAMPLE_COUNT = 64;
		generateCubeMap(
			sheenTexture, MAX_MIP_LEVELS + 1,
			PARENT / "../shaders/ibl_filtering.frag",
			[&](CustomShader& shader, int mip) {
				auto roughness = (float)mip / (MAX_MIP_LEVELS - 1);
				shader.setUniformT("u_distribution", 2);
				shader.setUniformT("u_roughness", roughness);
				shader.setTexture("u_cubemapTexture", cubemapTexture, 0);
				shader.setUniformT("u_sampleCount", SHEEN_SAMPLE_COUNT);
				shader.setUniformT("u_width", SIZE);
				shader.setUniformT("u_lodBias", 0.0f);
				shader.setUniformT("u_isGeneratingLUT", 0);
				shader.setUniformT("u_floatTexture", 1);
				shader.setUniformT("u_intensityScale", 1.0f);
			});
	});

	sheenTexture.texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
		.setMinificationFilter(
			Magnum::SamplerFilter::Linear, Magnum::SamplerMipmap::Linear)
		.setMagnificationFilter(Magnum::SamplerFilter::Linear);
}

void EnvMap::draw(Magnum::ArcBall& camera) {
	Magnum::GL::Renderer::setDepthMask(GL_FALSE);
	_shader.setTexture("u_envCubeMap", cubemapTexture, 0)
		.setUniformT(
			"u_viewProjection", camera.projectionMatrix() * camera.viewMatrix())
		.setUniformT("u_Exposure", 1.0f)
		.draw(_mesh);
	Magnum::GL::Renderer::setDepthMask(GL_TRUE);
}
