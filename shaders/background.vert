#include "io.glsl"

uniform mat4 u_viewProjection;

void main() {
	gl_Position = vec4(a_position, 1);
	v_position = (inverse(mat4(mat3(u_viewProjection))) * gl_Position).xyz;
}
