#include "tonemapping.glsl"

in vec3 v_position;
//
uniform samplerCube u_envCubeMap;
//
out vec4 FragColor;

void main() {
	vec3 color = texture(u_envCubeMap, v_position * vec3(1, -1, 1)).rgb;

	// HDR tonemap and gamma correct
	color = toneMap_KhronosPbrNeutral(color);
	color = linearTosRGB(color);

	FragColor = vec4(color, 1.0);
}
