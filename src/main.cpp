#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/MeshData.h>

#include "env.hpp"
#include "loader.hpp"
#include "shader.hpp"
#include "window.hpp"

using namespace Magnum;

constexpr auto FB_SIZE = Vector2i(1024);

struct {
	Matrix4 transformationMatrix;
	Color3 color;
} instanceData[]{
	{Matrix4::translation({0, 0, 1}) * Matrix4::scaling({0.5, 0.5, 0.5}),
	 0xcd3431_rgbf},
	{Matrix4::translation({0, 0, 2}) * Matrix4::scaling({0.5, 0.5, 0.5}),
	 0xc7cf2f_rgbf},
	{Matrix4::translation({0, 0, 3}) * Matrix4::scaling({0.5, 0.5, 0.5}),
	 0x3bd267_rgbf},
};

struct Main {
	EnvMap environment;
	PbrGL shader;

	GL::Mesh mesh;

	Main() {
		environment.update("./images/abandoned_garage_4k.hdr");
		PbrGL::Flags flags;
		flags |= PbrGL::Flag::Instancing;
		flags |= PbrGL::Flag::VertexColor3;
		flags |= PbrGL::Flag::ImageBasedLighting;
		PbrGL::Configuration conf;
		conf.setFlags(flags);
		shader = PbrGL(conf);

		mesh = MeshTools::compile(Primitives::icosphereSolid(2));

		mesh.addVertexBufferInstanced(
				GL::Buffer{instanceData}, 1, 0,
				Shaders::PhongGL::TransformationMatrix{},
				Shaders::PhongGL::Color3{})
			.setInstanceCount(3);
	}

	void drawImgui() {
		using namespace ImGui;
		if (Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			Text(
				"Average %.3f ms/frame (%.1f FPS)",
				1000.0 / Double(GetIO().Framerate), Double(GetIO().Framerate));
		}

		End();
	}

	void draw(Magnum::ArcBall& camera) {
		environment.draw(camera);
		int textureId = 0;

		auto& env = environment;
		shader.setTexture("u_LambertianEnvSampler", env.diffuse(), textureId++);
		shader.setTexture("u_GGXEnvSampler", env.specular(), textureId++);
		shader.setTexture("u_GGXLUT", env.lut(), textureId++);
		shader.setUniformT("u_MipCount", 5);
		shader.setUniformT("u_EnvIntensity", 1.0f);
		shader.setUniformT("u_EnvRotation", Matrix3(Math::IdentityInit));

		shader.setUniformT("u_Exposure", 1.0f);
		shader.setUniformT("u_Camera", camera.position());
		shader.setUniformT(
			"u_ViewProjectionMatrix",
			camera.projectionMatrix() * camera.viewMatrix());

		shader.setUniformT("u_MetallicFactor", 1.0f);
		shader.setUniformT("u_BaseColorFactor", Color4(1));

		shader.draw(mesh);
	}

	void reset() {}

	bool keyPressEvent(KeyEvent& event) {
		using Key = Magnum::Platform::Application::Key;
		using Modifier = Magnum::Platform::Application::Modifier;

		// if (event.key() == Key::O && (event.modifiers() & Modifier::Ctrl)) {
		// 	return true;
		// }
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
		Magnum::App<Main> app({argc, argv}, "Main");

		return app.exec();
	}
	CPPTRACE_CATCH(const std::exception& e) {
		SPDLOG_CRITICAL("Exception: {}", e.what());
		cpptrace::from_current_exception().print();
	}
}
