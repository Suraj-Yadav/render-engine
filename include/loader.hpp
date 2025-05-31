#pragma once

#include <Magnum/GL/CubeMapTexture.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>

#include <filesystem>
#include <functional>
#include <vector>

#include "shader.hpp"

using namespace std::literals;
using namespace Magnum::Math::Literals;

// inline std::filesystem::path operator""_p(const char* __str, size_t __len) {
// 	return std::filesystem::path{__str};
// }

struct Img2D {
	Magnum::GL::Texture2D texture;
	Magnum::Vector2i size;
	Magnum::PixelFormat format;
};

struct ImgCubeMap {
	Magnum::GL::CubeMapTexture texture;
	Magnum::Vector2i size;
	Magnum::PixelFormat format;
};

Img2D loadImage(const std::filesystem::path& path);

ImgCubeMap loadCubeMap(
	const std::filesystem::path& px, const std::filesystem::path& nx,
	const std::filesystem::path& py, const std::filesystem::path& ny,
	const std::filesystem::path& pz, const std::filesystem::path& nz);

void generateImg2D(
	Img2D& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const std::function<void(CustomShader&, int)>& setup);

void generateCubeMap(
	ImgCubeMap& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const std::function<void(CustomShader&, int)>& setup);

bool save(Img2D& img, const std::filesystem::path& path);

bool save(ImgCubeMap& img, const std::filesystem::path& path);

struct MaterialInfo {
	Magnum::Color3 baseColor = Magnum::Color3(1), emission = Magnum::Color3(0);
	float metalness = 1, roughness = 1, ao = 1;
	int baseColorTexture = -1, metalnessTexture = -1, roughnessTexture = -1,
		aoTexture = -1, emissionTexture = -1, normalTexture = -1;
};

struct DrawInfo {
	std::vector<Magnum::GL::Mesh> meshs;
	std::vector<Img2D> textures;
	std::vector<MaterialInfo> materials;
	std::vector<std::pair<int, int>> meshMatPairs;
};

DrawInfo loadMesh(const std::filesystem::path& path);
