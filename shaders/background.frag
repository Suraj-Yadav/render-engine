#include "tonemapping.glsl"

in vec3 v_position;
//
uniform samplerCube u_envCubeMap;
//
out vec4 FragColor;

void main() {
	vec3 color = texture(u_envCubeMap, v_position).rgb;

	// HDR tonemap and gamma correct
	FragColor = vec4(toneMap(color), 1.0);
}
