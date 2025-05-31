#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/DualQuaternion.h>

/* Implementation of Ken Shoemake's arcball camera with smooth
   navigation feature:
   https://www.talisman.org/~erlkonig/misc/shoemake92-arcball.pdf */
namespace Magnum {
	class ArcBall {
	   public:
		ArcBall() = default;
		ArcBall(
			const Vector3& eye, const Vector3& viewCenter, const Vector3& upDir,
			Magnum::Deg fov, const Vector2i& windowSize);

		void setViewParameters(
			const Vector3& eye, const Vector3& viewCenter,
			const Vector3& upDir);

		void reset();

		void reshape(
			const Vector2i& windowSize, const Vector2i& framebufferSize) {
			_windowSize = windowSize;
			_projection = Matrix4::perspectiveProjection(
				_fov, Vector2{framebufferSize}.aspectRatio(), 0.001f,
				Math::Constants<float>::inf());
			_projectionInv = _projection.inverted();

			updateInternalTransformations();
		}

		bool updateTransformation();

		[[nodiscard]] float lagging() const { return _lagging; }
		void setLagging(float lagging);

		void initTransformation(const Vector2& pointerPosition);

		void rotate(const Vector2& pointerPosition);

		void translate(const Vector2& pointerPosition);

		void translateDelta(const Vector2& translationNdc);

		void zoom(float delta);

		[[nodiscard]] auto position() {
			return _inverseView.transformPoint(_currentPosition);
		}

		[[nodiscard]] Magnum::Deg fov() const { return _fov; }

		[[nodiscard]] const Magnum::DualQuaternion& view() const {
			return _view;
		}

		[[nodiscard]] Matrix4 viewMatrix() const { return _view.toMatrix(); }

		[[nodiscard]] Matrix4 inverseViewMatrix() const {
			return _inverseView.toMatrix();
		}

		[[nodiscard]] Matrix4 projectionMatrix() const { return _projection; }

		[[nodiscard]] Matrix4 invProjectionMatrix() const {
			return _projectionInv;
		}

		[[nodiscard]] const Magnum::DualQuaternion& transformation() const {
			return _inverseView;
		}

		[[nodiscard]] Matrix4 transformationMatrix() const {
			return _inverseView.toMatrix();
		}

		[[nodiscard]] float viewDistance() const {
			return Magnum::Math::abs(_targetZooming);
		}

		[[nodiscard]] auto screenCoordToWorld(const Vector2& pos) const {
			const Vector2i viewSize = _windowSize;
			const Vector2 viewPosition{pos.x(), viewSize.y() - pos.y() - 1};

			const Vector3 start{
				2.0f * viewPosition / Vector2{viewSize} - Vector2{1.0f}, -1};
			const Vector3 end{start.x(), start.y(), 1};

			const auto mat = inverseViewMatrix() * invProjectionMatrix();

			auto s = mat.transformPoint(start);
			auto e = mat.transformPoint(end);
			return std::make_pair(s, (e - s).normalized());
		}

	   protected:
		void updateInternalTransformations();

		[[nodiscard]] Vector2 screenCoordToNdc(
			const Vector2& pointerPosition) const;

		Magnum::Deg _fov;
		Vector2i _windowSize;

		Vector2 _prevPointerPositionNdc;
		float _lagging{};

		Matrix4 _projection, _projectionInv;

		Vector3 _targetPosition, _currentPosition, _positionT0;
		Magnum::Quaternion _targetQRotation, _currentQRotation, _qRotationT0;
		float _targetZooming{}, _currentZooming{}, _zoomingT0{};
		Magnum::DualQuaternion _view, _inverseView;
	};
}  // namespace Magnum
