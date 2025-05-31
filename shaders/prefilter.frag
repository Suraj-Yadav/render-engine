#include "math.glsl"
#include "pbr.glsl"

in vec3 v_position;

uniform samplerCube u_textureData;
uniform float u_roughness;
//
out vec4 FragColor;

// ----------------------------------------------------------------------------
void main() {
	vec3 N = normalize(v_position.xyz);

	// make the simplifying assumption that V equals R equals the normal
	vec3 R = N;
	vec3 V = R;

	const uint SAMPLE_COUNT = 1024u;
	vec3 prefilteredColor = vec3(0.0);
	float totalWeight = 0.0;

	for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
		// generates a sample vector that's biased towards the preferred
		// alignment direction (importance sampling).
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H = ImportanceSampleGGX(Xi, N, u_roughness);
		vec3 L = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0);
		if (NdotL > 0.0) {
			// sample from the environment's mip level based on roughness/pdf
			float D = DistributionGGX(N, H, u_roughness);
			float NdotH = max(dot(N, H), 0.0);
			float HdotV = max(dot(H, V), 0.0);
			float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

			float resolution =
				512.0;	// resolution of source cubemap (per face)
			float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
			float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

			float mipLevel =
				u_roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

			prefilteredColor +=
				textureLod(u_textureData, L, mipLevel).rgb * NdotL;
			totalWeight += NdotL;
		}
	}

	prefilteredColor = prefilteredColor / totalWeight;

	FragColor = vec4(prefilteredColor, 1.0);
}
