#include "image.hpp"

#include <Corrade/PluginManager/Manager.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/Square.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>

#include <filesystem>
#include <ranges>

#include "logging.hpp"

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
		output, maxMipLevels, PARENT / "../shaders/image.vert", fragPath,
		setup);
}

void generateCubeMap(
	ImgCubeMap& output, int maxMipLevels, const std::filesystem::path& fragPath,
	const std::function<void(CustomShader&, int)>& setup) {
	generate<ImgCubeMap, CUBE_TRANSFORMS.size()>(
		output, maxMipLevels, PARENT / "../shaders/cubemap.vert", fragPath,
		setup);
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

template <typename T> void read(std::ifstream& file, T& v) {
	file.read(reinterpret_cast<char*>(&v), sizeof(v));
}
template <typename T> void write(std::ofstream& file, T& v) {
	file.write(reinterpret_cast<char*>(&v), sizeof(v));
}

template <typename T> void write(T& img, const std::filesystem::path& path) {
	std::ofstream file(path, std::ios::binary);

	write(file, img.size.x());
	write(file, img.size.y());
	write(file, img.format);
	write(file, img.levels);

	auto size = img.size;
	auto format = img.format;

	if constexpr (std::same_as<T, Img2D>) {
		for (auto i = 0; i < img.levels; ++i) {
			size = img.texture.imageSize(i);
			write(file, size.x());
			write(file, size.y());
			Image2D data{format};
			img.texture.image(i, data);
			file.write(data.data().data(), data.data().size());
		}
	}
	if constexpr (std::same_as<T, ImgCubeMap>) {
		for (auto i = 0; i < img.levels; ++i) {
			size = img.texture.imageSize(i);
			write(file, size.x());
			write(file, size.y());
			for (const auto& e : CUBE_MAP_COORDS) {
				Image2D data{format};
				img.texture.image(e, i, data);
				file.write(data.data().data(), data.data().size());
			}
		}
	}
}

template <typename T> void read(T& img, const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);

	read(file, img.size.x());
	read(file, img.size.y());
	read(file, img.format);
	read(file, img.levels);
	img = {img.size, img.format, img.levels};
	img.setStorage();

	auto size = img.size;
	auto format = img.format;

	if constexpr (std::same_as<T, Img2D>) {
		for (auto i = 0; i < img.levels; ++i) {
			read(file, size.x());
			read(file, size.y());
			Image2D data{
				format, size,
				Containers::Array<char>{
					ValueInit,
					std::size_t(size.product() * pixelFormatSize(format))}};

			file.read(data.data().data(), data.data().size());
			img.texture.setSubImage(i, {}, data);
		}
	}
	if constexpr (std::same_as<T, ImgCubeMap>) {
		for (auto i = 0; i < img.levels; ++i) {
			read(file, size.x());
			read(file, size.y());
			for (const auto& e : CUBE_MAP_COORDS) {
				Image2D data{
					format, size,
					Containers::Array<char>{
						ValueInit,
						std::size_t(size.product() * pixelFormatSize(format))}};

				file.read(data.data().data(), data.data().size());
				img.texture.setSubImage(e, i, {}, data);
			}
		}
	}
}

template <typename T>
void cacheImpl(
	T& img, const std::filesystem::path& path,
	const std::function<void()>& generator) {
	if (std::filesystem::exists(path)) {
		read(img, path);
		return;
	}
	generator();
	write(img, path);
}

void cache(
	Img2D& img, const std::filesystem::path& path,
	const std::function<void()>& generator) {
	cacheImpl(img, path, generator);
}

void cache(
	ImgCubeMap& img, const std::filesystem::path& path,
	const std::function<void()>& generator) {
	cacheImpl(img, path, generator);
}
