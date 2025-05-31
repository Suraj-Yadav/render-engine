#include "io.glsl"

uniform mat4 u_transform;

void main() {
	v_position = vec3(u_transform * vec4(a_position, 1));
	gl_Position = vec4(a_position, 1);
}
