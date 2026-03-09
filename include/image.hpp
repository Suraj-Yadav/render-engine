#pragma once

#include <Magnum/GL/CubeMapTexture.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>

#include <filesystem>
#include <functional>

#include "shader.hpp"

template <typename T> struct Img {
	Magnum::Vector2i size;
	Magnum::PixelFormat format;
	int levels{};
	T texture;

	Img() = default;
	Img(T&& img, Magnum::Vector2i sz, Magnum::PixelFormat fmt, int lvls = 1)
		: texture(std::move(img)), size(sz), format(fmt), levels(lvls) {}
	Img(int sz, Magnum::PixelFormat fmt, int lvls = 1)
		: size(sz), format(fmt), levels(lvls) {}
	Img(Magnum::Vector2i sz, Magnum::PixelFormat fmt, int lvls = 1)
		: size(sz), format(fmt), levels(lvls) {}

	auto& setStorage() {
		texture.setStorage(levels, Magnum::GL::textureFormat(format), size);
		if (levels > 1) { texture.generateMipmap(); }
		return texture;
	}
};

using Img2D = Img<Magnum::GL::Texture2D>;
using ImgCubeMap = Img<Magnum::GL::CubeMapTexture>;

Img2D loadImage(const std::filesystem::path& path);

ImgCubeMap loadCubeMap(
	const std::filesystem::path& px, const std::filesystem::path& nx,
	const std::filesystem::path& py, const std::filesystem::path& ny,
	const std::filesystem::path& pz, const std::filesystem::path& nz);

using GENERATOR_SETUP = std::function<void(CustomShader&, int)>;

void generateImg2D(
	Img2D& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const GENERATOR_SETUP& setup);

void generateCubeMap(
	ImgCubeMap& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const GENERATOR_SETUP& setup);

bool save(Img2D& img, const std::filesystem::path& path);

bool save(ImgCubeMap& img, const std::filesystem::path& path);

void cache(
	Img2D& img, const std::filesystem::path& path,
	const std::function<void()>& generator);

void cache(
	ImgCubeMap& img, const std::filesystem::path& path,
	const std::function<void()>& generator);
