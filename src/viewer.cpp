#include <Corrade/Containers/StructuredBindings.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/MeshData.h>
#include <imgui.h>
#include <tinyfiledialogs.h>

#include "env.hpp"
#include "loader.hpp"
#include "shader.hpp"
#include "window.hpp"

std::optional<std::filesystem::path> openFile() {
	auto* selection =
		tinyfd_openFileDialog("Open File", nullptr, 0, nullptr, nullptr, 0);
	if (selection == nullptr) { return {}; }
	return selection;
}

using namespace Magnum;

constexpr auto FB_SIZE = Vector2i(1024);

struct Main {
	EnvMap environment;
	DrawInfo drawInfo;

	int view = 0;
	std::vector<Light> lights;

	Vector4 input = Vector4(90, 0, 270, 1);
	Matrix4 model;

	GL::Framebuffer opaqueFramebuffer{{{}, FB_SIZE}};
	GL::Renderbuffer opaqueDepth;
	Img2D opaque;

	std::map<int, PbrGL> shaders;

	Main() : opaque(FB_SIZE, Magnum::PixelFormat::RGBA8Unorm, 5) {
		opaque.setStorage()
			.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
			.setMinificationFilter(
				Magnum::SamplerFilter::Linear, Magnum::SamplerMipmap::Linear)
			.setMagnificationFilter(Magnum::SamplerFilter::Nearest);

		opaqueDepth.setStorage(
			GL::RenderbufferFormat::DepthComponent24, FB_SIZE);

		opaqueFramebuffer.setViewport({{}, FB_SIZE});
		opaqueFramebuffer.attachRenderbuffer(
			GL::Framebuffer::BufferAttachment::Depth, opaqueDepth);
		opaqueFramebuffer.attachTexture(
			GL::Framebuffer::ColorAttachment{0}, opaque.texture, 0);

		ASSERT_MESG(
			opaqueFramebuffer.checkStatus(GL::FramebufferTarget::Draw) ==
				GL::Framebuffer::Status::Complete,
			"status = {}",
			int(opaqueFramebuffer.checkStatus(GL::FramebufferTarget::Draw)));

		environment.update("./images/abandoned_garage_4k.hdr");
	}

	void updateModel() {
		model = Matrix4::rotationZ(Math::Deg(input.z())) *
				Matrix4::rotationY(Math::Deg(input.y())) *
				Matrix4::rotationX(Math::Deg(input.x())) *
				Matrix4::scaling(Vector3(input.w()));
	}

	auto createShader(int matId) {
		if (shaders.contains(matId)) { return false; }
		auto& mat = drawInfo.materials[matId];
		PbrGL::Configuration conf;
		auto flags = mat.flags;
		flags |= PbrGL::Flag::ImageBasedLighting;
		if (lights.size() > 0) { flags |= PbrGL::Flag::PunctualLights; }
		conf.setView(view).setLightCount(lights.size()).setFlags(flags);
		shaders.emplace(matId, conf);
		return true;
	}

	void updateShaders() { shaders.clear(); }

	void load() {
		auto path = openFile();
		if (!path) { return; }

		drawInfo = loadMesh(*path);

		updateShaders();
		updateModel();
	}

	void drawImgui() {
		using namespace ImGui;
		bool changeShader = false;
		if (Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			Text(
				"Average %.3f ms/frame (%.1f FPS)",
				1000.0 / Double(GetIO().Framerate), Double(GetIO().Framerate));

			{
				bool changed = DragFloat3("Rotation", input.data(), 15, 0, 360);
				changed |= SliderFloat(
					"Scale", &input.w(), 0.0001, 10, "%.3f",
					ImGuiSliderFlags_Logarithmic);
				if (Button("Reset Model")) {
					changed = true;
					input = Vector4(0, 0, 0, 1);
				}
				if (changed) { updateModel(); }
			}

			if (Combo("View", &view, VIEW_OPTIONS)) { changeShader = true; }

			{
				if (Button("Change Background")) {
					auto path = openFile();
					if (path) {
						changeShader = true;
						environment.update(*path);
					}
				}
			}

			{
				bool useLights = lights.size() > 0;
				if (Checkbox("Use Lights", &useLights)) {
					if (!useLights) {
						changeShader = true;
						lights.clear();
					}
				}
				for (auto& l : lights) {
					PushID(&l);
					Separator();
					PushItemWidth(80);
					Combo("T", &l.type, LIGHT_TYPES);
					SameLine();
					ColorEdit3(
						"C", l.color.data(), ImGuiColorEditFlags_NoInputs);
					SameLine();
					DragFloat3("P", l.position.data());
					SameLine();
					InputFloat("I", &l.intensity);
					PopItemWidth();
					PopID();
				}
				if (Button("Add Light")) {
					lights.emplace_back();
					changeShader = true;
				}
			}
		}

		End();
		if (changeShader) { updateShaders(); }
	}

	void draw(
		Magnum::ArcBall& camera, bool drawOpaqueOnly, int meshId, int matId,
		Matrix4& transform) {
		auto& mesh = drawInfo.meshs[meshId];
		auto& mat = drawInfo.materials[matId];

		if (!mat.opaque && drawOpaqueOnly) { return; }

		auto created = createShader(matId);
		auto& shader = shaders[matId];

		if (mat.doubleSided) {
			GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
		} else {
			GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
		}
		int textureId = 0;

		auto& env = environment;
		shader.setTexture("u_LambertianEnvSampler", env.diffuse(), textureId++);
		shader.setTexture("u_GGXEnvSampler", env.specular(), textureId++);
		shader.setTexture("u_GGXLUT", env.lut(), textureId++);

		if (mat.flags >= PbrGL::Flag::MaterialSheen) {
			shader.setTexture("u_CharlieEnvSampler", env.sheen(), textureId++);
			shader.setTexture("u_CharlieLUT", env.sheenLUT(), textureId++);
			shader.setTexture("u_SheenELUT", env.sheenELUT(), textureId++);
		}
		shader.setUniformT("u_MipCount", 5);
		shader.setUniformT("u_EnvIntensity", 1.0f);
		shader.setUniformT("u_EnvRotation", Matrix3(Math::IdentityInit));

		shader.setLights(lights);

		// Set camera Stuff
		shader.setUniformT("u_Exposure", 1.0f);
		shader.setUniformT("u_Camera", camera.position());
		shader.setUniformT(
			"u_ViewProjectionMatrix",
			camera.projectionMatrix() * camera.viewMatrix());

		for (auto& [u, v] : mat.colors) {
			shader.setUniformT(std::string(u), v);
		}
		for (auto& [u, v] : mat.floats) {
			shader.setUniformT(std::string(u), v);
		}
		for (auto& [u, v] : mat.transforms) {
			shader.setUniformT(std::string(u), v);
		}
		shader.setUniformT(
			std::string(mat.baseColor.uniform), mat.baseColor.value);
		shader.setUniformT(
			std::string(mat.metallic.uniform), mat.metallic.value);
		shader.setUniformT(
			std::string(mat.roughness.uniform), mat.roughness.value);

		for (auto& [u, v] : mat.textures) {
			shader.setTexture(
				std::string(u), drawInfo.textures[v], textureId++);
		}

		auto mod = model * transform;

		shader.setUniformT("u_ModelMatrix", mod)
			.setUniformT("u_NormalMatrix", Matrix4(mod.normalMatrix()));

		if (!mat.opaque) {
			shader.setUniformT("u_ViewMatrix", camera.viewMatrix());
			shader.setUniformT("u_ProjectionMatrix", camera.projectionMatrix());
			shader.setTexture(
				"u_TransmissionFramebufferSampler", opaque, textureId++);
			shader.setUniformT("u_TransmissionFramebufferSize", FB_SIZE);
		}

		if (created) {
			auto [ok, msg] = shader.validate();
			ASSERT_MESG(ok, msg);
		}

		shader.draw(mesh);
	}

	void draw(Magnum::ArcBall& camera) {
		auto allOpaque = std::ranges::all_of(drawInfo.nodes, [&](auto& e) {
			return drawInfo.materials[std::get<1>(e)].opaque;
		});

		auto drawScene = [&](bool drawOpaqueOnly) {
			for (auto& [meshId, matId, transform] : drawInfo.nodes) {
				draw(camera, drawOpaqueOnly, meshId, matId, transform);
			}
			environment.draw(camera);
		};

		if (!allOpaque) {
			opaqueFramebuffer.bind();
			opaqueFramebuffer.clear(
				GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

			drawScene(true);
			GL::defaultFramebuffer.bind();

			opaque.texture.generateMipmap();
		}

		drawScene(false);
	}

	void reset() {}

	bool keyPressEvent(KeyEvent& event) {
		using Key = Magnum::Platform::Application::Key;
		using Modifier = Magnum::Platform::Application::Modifier;

		if (event.key() == Key::O && (event.modifiers() & Modifier::Ctrl)) {
			load();
			return true;
		}
		if (event.key() == Key::Space) {}
		return false;
	}
	void pointerPressEvent(PointerEvent& event, Magnum::ArcBall& _camera) {}

	void pointerReleaseEvent(PointerEvent& event, Magnum::ArcBall& _camera) {
		// _camera.screenCoordToWorld(event.position());
		using Modifier = Magnum::Platform::Application::Modifier;

		if (event.modifiers() & Modifier::Alt) {}
	}

	void pointerMoveEvent(PointerMoveEvent& event, Magnum::ArcBall& _camera) {}
};

int main(int argc, char** argv) {
	spdlog::cfg::load_env_levels();

	CPPTRACE_TRY {
		// test();
		Magnum::App<Main> app({argc, argv}, "GLTF/GLB Viewer");
		return app.exec();
	}
	CPPTRACE_CATCH(const std::exception& e) {
		SPDLOG_CRITICAL("Exception: {}", e.what());
		cpptrace::from_current_exception().print();
	}
}
