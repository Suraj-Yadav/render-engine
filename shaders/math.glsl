const float PI = 3.14159265359;

mat4 translation(vec3 delta) {
	return mat4(
		vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(delta, 1));
}

mat4 rotation(float angle, vec3 axis) {
	axis = normalize(axis);
	float s = sin(radians(angle));
	float c = cos(radians(angle));
	float oc = 1.0 - c;

	return mat4(
		oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,
		oc * axis.z * axis.x + axis.y * s, 0.0,
		oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c,
		oc * axis.y * axis.z - axis.x * s, 0.0,
		oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s,
		oc * axis.z * axis.z + c, 0.0, 0.0, 0.0, 0.0, 1.0);
}
