#include "loader.hpp"

#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Format.h>
#include <Magnum/GL/CubeMapTexture.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/Square.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include <array>
#include <concepts>
#include <ranges>
#include <stdexcept>

#include "logging.hpp"
#include "shader.hpp"

using namespace Magnum;
using namespace Magnum::Math::Literals;

auto load(const std::filesystem::path& path) {
	static PluginManager::Manager<Trade::AbstractImporter> importer;
	static Corrade::Containers::Pointer<Trade::AbstractImporter> imageImporter(
		importer.loadAndInstantiate("AnyImageImporter"));
	if (!imageImporter) {
		throw std::runtime_error("Unable to initialize image importer");
	}

	imageImporter->openFile(path.string());
	auto image = imageImporter->image2D(0);
	CORRADE_INTERNAL_ASSERT(image);

	auto img = std::move(*image);
	imageImporter->close();

	return img;
}

Img2D loadImage(const std::filesystem::path& path) {
	auto image = load(path);
	auto size = image.size();
	auto format = image.format();

	GL::Texture2D texture;
	texture.setStorage(1, GL::textureFormat(format), size)
		.setSubImage(0, {}, image)
		.setMinificationFilter(SamplerFilter::Linear)
		.setMagnificationFilter(SamplerFilter::Linear)
		.setWrapping(GL::SamplerWrapping::ClampToEdge);

	return {std::move(texture), size, format};
}

ImgCubeMap loadCubeMap(
	const std::filesystem::path& px, const std::filesystem::path& nx,
	const std::filesystem::path& py, const std::filesystem::path& ny,
	const std::filesystem::path& pz, const std::filesystem::path& nz) {
	GL::CubeMapTexture texture;
	texture.setMinificationFilter(SamplerFilter::Linear)
		.setMagnificationFilter(SamplerFilter::Linear)
		.setWrapping(GL::SamplerWrapping::ClampToEdge);

	auto image = load(px);
	auto size = image.size();
	auto format = image.format();

	texture.setStorage(1, GL::textureFormat(format), size)
		.setSubImage(GL::CubeMapCoordinate::PositiveX, 0, {}, image);

	image = load(nx);
	texture.setSubImage(GL::CubeMapCoordinate::NegativeX, 0, {}, image);

	image = load(py);
	texture.setSubImage(GL::CubeMapCoordinate::PositiveY, 0, {}, image);

	image = load(ny);
	texture.setSubImage(GL::CubeMapCoordinate::NegativeY, 0, {}, image);

	image = load(pz);
	texture.setSubImage(GL::CubeMapCoordinate::PositiveZ, 0, {}, image);

	image = load(nz);
	texture.setSubImage(GL::CubeMapCoordinate::NegativeZ, 0, {}, image);

	return {std::move(texture), size, format};
}

const auto CUBE_MAP_COORDS = std::to_array({
	GL::CubeMapCoordinate::PositiveX,
	GL::CubeMapCoordinate::NegativeX,
	GL::CubeMapCoordinate::PositiveY,
	GL::CubeMapCoordinate::NegativeY,
	GL::CubeMapCoordinate::PositiveZ,
	GL::CubeMapCoordinate::NegativeZ,
});

const auto CUBE_TRANSFORMS = std::to_array({
	Matrix4::rotation(90.0_degf, Vector3::yAxis()) *
		Matrix4::translation(Vector3::zAxis()),
	Matrix4::rotation(270.0_degf, Vector3::yAxis()) *
		Matrix4::translation(Vector3::zAxis()),
	Matrix4::rotation(90.0_degf, Vector3::xAxis()) *
		Matrix4::translation(Vector3::zAxis()),
	Matrix4::rotation(270.0_degf, Vector3::xAxis()) *
		Matrix4::translation(Vector3::zAxis()),
	Matrix4::translation(Vector3::zAxis()),
	Matrix4::rotation(180.0_degf, Vector3::yAxis()) *
		Matrix4::translation(Vector3::zAxis()),
});

using GENERATOR_SETUP = std::function<void(CustomShader&, int)>;

template <typename ImgType, std::size_t LIMIT>
void generate(
	ImgType& output, int maxMipLevels, const std::filesystem::path& vertPath,
	const std::filesystem::path& fragPath, const GENERATOR_SETUP& setup) {
	//

	CustomShader shader(vertPath, fragPath);

	GL::Renderbuffer depthStencil;
	depthStencil.setStorage(
		GL::RenderbufferFormat::DepthComponent24, output.size);

	GL::Framebuffer fb{{{}, output.size}};

	fb.setViewport({{}, output.size});

	fb.bind();

	fb.attachRenderbuffer(
		GL::Framebuffer::BufferAttachment::Depth, depthStencil);

	auto mesh = MeshTools::compile(Primitives::squareSolid());

	for (auto mip = 0; mip < maxMipLevels; ++mip) {
		fb.setViewport({{}, output.size / (1 << mip)});

		setup(shader, mip);

		for (auto i = 0u; i < std::min(LIMIT, CUBE_MAP_COORDS.size()); ++i) {
			if constexpr (std::same_as<ImgType, Img2D>) {
				fb.attachTexture(
					GL::Framebuffer::ColorAttachment{0}, output.texture, mip);
			}
			if constexpr (std::same_as<ImgType, ImgCubeMap>) {
				fb.attachCubeMapTexture(
					GL::Framebuffer::ColorAttachment{0}, output.texture,
					CUBE_MAP_COORDS.at(i), mip);
				shader.setUniformT("u_transform", CUBE_TRANSFORMS.at(i));
			}
			ASSERT_MESG(
				fb.checkStatus(GL::FramebufferTarget::Draw) ==
					GL::Framebuffer::Status::Complete,
				"mip = {}, i = {}, status = {}", mip, i,
				int(fb.checkStatus(GL::FramebufferTarget::Draw)));

			fb.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth)
				.bind();

			shader.draw(mesh);
		}
	}

	GL::defaultFramebuffer.bind();
}

void generateImg2D(
	Img2D& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const std::function<void(CustomShader&, int)>& setup) {
	generate<Img2D, 1>(
		output, maxMipLevels, "./shaders/image.vert", fragPath, setup);
}

void generateCubeMap(
	ImgCubeMap& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const std::function<void(CustomShader&, int)>& setup) {
	generate<ImgCubeMap, CUBE_TRANSFORMS.size()>(
		output, maxMipLevels, "./shaders/cubemap.vert", fragPath, setup);
}

auto save(const Image2D& imageData, const std::filesystem::path& path) {
	static Magnum::PluginManager::Manager<Trade::AbstractImageConverter>
		converter;
	static Containers::Pointer<Trade::AbstractImageConverter> imageConverter(
		converter.loadAndInstantiate("AnyImageConverter"));

	if (!imageConverter) {
		throw std::runtime_error("Unable to initialize image importer");
	}

	return imageConverter->convertToFile(imageData, path.string());
}

bool save(Img2D& img, const std::filesystem::path& path) {
	Image2D out(img.format);
	img.texture.image(0, out);
	return save(out, path);
}

bool save(ImgCubeMap& img, const std::filesystem::path& path) {
	constexpr auto SUFFIXS =
		std::to_array({"_px", "_nx", "_py", "_ny", "_pz", "_nz"});

	Image2D out(img.format);
	for (const auto& [suffix, coord] :
		 std::views::zip(SUFFIXS, CUBE_MAP_COORDS)) {
		auto p = path;
		p.replace_extension();
		p += suffix;
		p += path.extension();
		img.texture.image(coord, 0, out);
		save(out, p);
		break;
	}
	return true;
}

DrawInfo loadMesh(const std::filesystem::path& path) {
	using Corrade::Utility::format;

	static Magnum::PluginManager::Manager<Trade::AbstractImporter> manager;
	static Containers::Pointer<Trade::AbstractImporter> importer(
		manager.loadAndInstantiate("AnySceneImporter"));

	if (!importer) {
		throw std::runtime_error("Unable to initialize mesh importer");
	}

	if (!importer->openFile(path.string())) {
		throw std::runtime_error("Unable to load mesh");
	}

	DrawInfo info;

	for (auto i = 0u; i < importer->textureCount(); ++i) {
		auto textureData = importer->texture(i);
		using Corrade::Utility::format;

		if (!textureData ||
			textureData->type() != Trade::TextureType::Texture2D) {
			throw std::runtime_error(
				format("Invalid Texture: {}: {}", i, importer->textureName(i)));
		}
		auto data = importer->image2D(textureData->image());

		if (!data || data->isCompressed()) {
			throw std::runtime_error(format(
				"Cannot load image: {}: {}", textureData->image(),
				importer->image2DName(textureData->image())));
		}

		GL::Texture2D tex;
		tex.setMagnificationFilter(textureData->magnificationFilter())
			.setMinificationFilter(
				textureData->minificationFilter(), textureData->mipmapFilter())
			.setWrapping(textureData->wrapping().xy())
			.setStorage(
				Math::log2(data->size().max()) + 1,
				GL::textureFormat(data->format()), data->size())
			.setSubImage(0, {}, *data)
			.generateMipmap();
		info.textures.emplace_back(
			std::move(tex), data->size(), data->format());
	}

	for (auto i = 0u; i != importer->materialCount(); ++i) {
		if (!importer->material(i)) {
			throw std::runtime_error(
				format("Cannot load Material: {}", importer->materialName(i)));
		}

		if (!(importer->material(i)->types() >=
			  Trade::MaterialType::PbrMetallicRoughness)) {
			throw std::runtime_error(format(
				"Material {} is not PbrMetallicRoughness",
				importer->materialName(i)));
		}

		auto mat = std::move(*importer->material(i))
					   .as<Trade::PbrMetallicRoughnessMaterialData>();

		MaterialInfo matInfo;

		using Trade::MaterialAttribute;

#define assign(A, L, R) \
	if (mat.hasAttribute(Trade::MaterialAttribute::A)) { matInfo.L = mat.R; }

		assign(BaseColor, baseColor, baseColor().rgb());
		assign(BaseColorTexture, baseColorTexture, baseColorTexture());
		assign(Metalness, metalness, metalness());
		assign(Roughness, roughness, roughness());
		assign(MetalnessTexture, metalnessTexture, metalnessTexture());
		assign(RoughnessTexture, roughnessTexture, roughnessTexture());
		assign(
			NoneRoughnessMetallicTexture, metalnessTexture, metalnessTexture());
		assign(
			NoneRoughnessMetallicTexture, roughnessTexture, roughnessTexture());
		assign(EmissiveColor, emission, emissiveColor());
		assign(EmissiveTexture, emissionTexture, emissiveTexture());
		assign(OcclusionTexture, aoTexture, occlusionTexture());
		assign(NormalTexture, normalTexture, normalTexture());
#undef assign

		info.materials.push_back(std::move(matInfo));
	}

	for (auto i = 0u; i != importer->meshCount(); ++i) {
		auto meshData = importer->mesh(i);
		if (!meshData) {
			throw std::runtime_error(
				format("Cannot load image: {}: {}", i, importer->meshName(i)));
		}
		MeshTools::CompileFlags flags;
		if (!meshData->hasAttribute(Trade::MeshAttribute::Normal)) {
			flags |= MeshTools::CompileFlag::GenerateFlatNormals;
		}
		info.meshs.emplace_back(MeshTools::compile(*meshData, flags));
	}

	auto scene = importer->scene(importer->defaultScene());
	if (!scene || !scene->is3D()) {
		throw std::runtime_error(format(
			"Cannot load image: {}: {}", importer->defaultScene(),
			importer->sceneName(importer->defaultScene())));
	}
	for (const auto& elem : scene->meshesMaterialsAsArray()) {
		auto mesh = elem.second().first();
		auto mat = elem.second().second();
		info.meshMatPairs.emplace_back(mesh, mat);
	}
	return info;
}
