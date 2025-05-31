#define HAS_NORMAL_VEC3
#define HAS_TEXCOORD_0_VEC2

#include "io.glsl"

//
uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat3 u_normal;

void main() {
	v_position = vec3(u_model * vec4(a_position, 1));
	v_normal = u_normal * a_normal;
	v_texcoord = a_texcoord;
	gl_Position = u_projection * u_view * vec4(v_position, 1);
}
