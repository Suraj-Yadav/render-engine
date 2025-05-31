#include "camera.hpp"

#include <Corrade/Utility/Assert.h>

using namespace Magnum;

/* Project a point in NDC onto the arcball sphere */
Quaternion ndcToArcBall(const Vector2& p) {
	const Float dist = Math::dot(p, p);

	/* Point is on sphere */
	if (dist <= 1.0f) {
		return {{p.x(), p.y(), Math::sqrt(1.0f - dist)}, 0.0f};

		/* Point is outside sphere */
	}
	return {{p.normalized(), 0.0f}, 0.0f};
}

ArcBall::ArcBall(
	const Vector3& eye, const Vector3& viewCenter, const Vector3& upDir,
	Deg fov, const Vector2i& windowSize)
	: _fov{fov}, _windowSize{windowSize} {
	setViewParameters(eye, viewCenter, upDir);
}

void ArcBall::setViewParameters(
	const Vector3& eye, const Vector3& viewCenter, const Vector3& upDir) {
	const Vector3 dir = viewCenter - eye;
	Vector3 zAxis = dir.normalized();
	Vector3 xAxis = (Math::cross(zAxis, upDir.normalized())).normalized();
	Vector3 yAxis = (Math::cross(xAxis, zAxis)).normalized();
	xAxis = (Math::cross(zAxis, yAxis)).normalized();

	_targetPosition = -viewCenter;
	_targetZooming = -dir.length();
	_targetQRotation =
		Quaternion::fromMatrix(Matrix3x3{xAxis, yAxis, -zAxis}.transposed())
			.normalized();

	_positionT0 = _currentPosition = _targetPosition;
	_zoomingT0 = _currentZooming = _targetZooming;
	_qRotationT0 = _currentQRotation = _targetQRotation;

	updateInternalTransformations();
}

void ArcBall::reset() {
	_targetPosition = _positionT0;
	_targetZooming = _zoomingT0;
	_targetQRotation = _qRotationT0;
}

void ArcBall::setLagging(const Float lagging) {
	CORRADE_INTERNAL_ASSERT(lagging >= 0.0f && lagging < 1.0f);
	_lagging = lagging;
}

void ArcBall::initTransformation(const Vector2& pointerPosition) {
	_prevPointerPositionNdc = screenCoordToNdc(pointerPosition);
}

void ArcBall::rotate(const Vector2& pointerPosition) {
	const Vector2 pointerPositionNdc = screenCoordToNdc(pointerPosition);
	const Quaternion currentQRotation = ndcToArcBall(pointerPositionNdc);
	const Quaternion prevQRotation = ndcToArcBall(_prevPointerPositionNdc);
	_prevPointerPositionNdc = pointerPositionNdc;
	_targetQRotation =
		(currentQRotation * prevQRotation * _targetQRotation).normalized();
}

void ArcBall::translate(const Vector2& pointerPosition) {
	const Vector2 mousePosNdc = screenCoordToNdc(pointerPosition);
	const Vector2 translationNdc = mousePosNdc - _prevPointerPositionNdc;
	_prevPointerPositionNdc = mousePosNdc;
	translateDelta(translationNdc);
}

void ArcBall::translateDelta(const Vector2& translationNdc) {
	/* Half size of the screen viewport at the view center and perpendicular
	   with the viewDir */
	const Float hh = Math::abs(_targetZooming) * Math::tan(_fov * 0.5f);
	const Float hw = hh * Vector2{_windowSize}.aspectRatio();

	_targetPosition += _inverseView.transformVector(
		{translationNdc.x() * hw, translationNdc.y() * hh, 0.0f});
}

void ArcBall::zoom(const Float delta) { _targetZooming += delta; }

bool ArcBall::updateTransformation() {
	const Vector3 diffViewCenter = _targetPosition - _currentPosition;
	const Quaternion diffRotation = _targetQRotation - _currentQRotation;
	const Float diffZooming = _targetZooming - _currentZooming;

	const Float dViewCenter = Math::dot(diffViewCenter, diffViewCenter);
	const Float dRotation = Math::dot(diffRotation, diffRotation);
	const Float dZooming = diffZooming * diffZooming;

	/* Nothing change */
	constexpr auto EPSILON1 = 1.0e-10f;
	if (dViewCenter < EPSILON1 && dRotation < EPSILON1 && dZooming < EPSILON1) {
		return false;
	}

	/* Nearly done: just jump directly to the target */
	constexpr auto EPSILON2 = 1.0e-10f;
	if (dViewCenter < EPSILON2 && dRotation < EPSILON2 && dZooming < EPSILON2) {
		_currentPosition = _targetPosition;
		_currentQRotation = _targetQRotation;
		_currentZooming = _targetZooming;

		/* Interpolate between the current transformation and the target
		   transformation */
	} else {
		const Float t = 1 - _lagging;
		_currentPosition = Math::lerp(_currentPosition, _targetPosition, t);
		_currentZooming = Math::lerp(_currentZooming, _targetZooming, t);
		_currentQRotation =
			Math::slerpShortestPath(_currentQRotation, _targetQRotation, t);
	}

	updateInternalTransformations();
	return true;
}

void ArcBall::updateInternalTransformations() {
	_view = DualQuaternion::translation(Vector3::zAxis(_currentZooming)) *
			DualQuaternion{_currentQRotation} *
			DualQuaternion::translation(_currentPosition);
	_inverseView = _view.inverted();
}

Vector2 ArcBall::screenCoordToNdc(const Vector2& pointerPosition) const {
	return Vector2::yScale(-1.0f) *
		   (pointerPosition * 2 / Vector2{_windowSize} - Vector2{1.0f});
}
