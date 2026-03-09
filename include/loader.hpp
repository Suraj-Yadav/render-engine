#pragma once

#include <Magnum/GL/Mesh.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>

#include <filesystem>
#include <vector>

#include "image.hpp"
#include "shader.hpp"

using namespace std::literals;
using namespace Magnum::Math::Literals;

struct MaterialInfo {
	template <typename T> struct Data {
		std::string_view uniform;
		T value;
	};

	bool doubleSided = false, opaque = true;
	Data<Magnum::Color4> baseColor;
	Data<float> metallic, roughness;

	std::vector<Data<Magnum::UnsignedInt>> textures;
	std::vector<Data<Magnum::Matrix3>> transforms;
	std::vector<Data<float>> floats;
	std::vector<Data<Magnum::Color3>> colors;

	PbrGL::Flags flags;
};

struct DrawInfo {
	std::vector<Magnum::GL::Mesh> meshs;
	std::vector<Img2D> textures;
	std::vector<MaterialInfo> materials;
	std::vector<std::tuple<int, int, Magnum::Matrix4>> nodes;
};

DrawInfo loadMesh(const std::filesystem::path& path);
