#include "math.glsl"
#include "pbr.glsl"

in vec3 v_position;
//
//
out vec4 FragColor;

// ----------------------------------------------------------------------------
void main() {
	vec3 pos = 0.5 * v_position + 0.5;
	vec2 integratedBRDF = IntegrateBRDF(pos.x, pos.y);
	FragColor = vec4(integratedBRDF, 0, 1);
}
