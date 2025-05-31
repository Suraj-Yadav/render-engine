#include "math.glsl"
#include "pbr.glsl"
#include "tonemapping.glsl"

in vec2 v_texcoord;
in vec3 v_normal;
in vec3 v_position;
//
uniform vec3 u_Camera;

uniform vec3 u_baseColor = vec3(1);
uniform float u_metalness = 1;
uniform float u_roughness = 1;
uniform float u_ao = 1;
uniform vec3 u_emissive = vec3(0);

#ifndef LIGHT_COUNT
#define LIGHT_COUNT 4
#endif

#ifdef HAS_BASECOLOR_TEXTURE
uniform sampler2D u_baseColorTexture;
#endif
#ifdef HAS_METALNESS_TEXTURE
uniform sampler2D u_metalnessTexture;
#endif
#ifdef HAS_ROUGHNESS_TEXTURE
uniform sampler2D u_roughnessTexture;
#endif
#ifdef HAS_AO_TEXTURE
uniform sampler2D u_aoTexture;
#endif
#ifdef HAS_NORMAL_TEXTURE
uniform sampler2D u_normalTexture;
#endif
#ifdef HAS_EMISSIVE_TEXTURE
uniform sampler2D u_emissiveTexture;
#endif
#ifdef HAS_IBL
uniform samplerCube u_irradianceMap;
uniform samplerCube u_prefilterMap;
uniform sampler2D u_brdfLUT;
#endif

// lights
uniform vec3 lightPositions[LIGHT_COUNT + 1];
uniform vec3 lightColors[LIGHT_COUNT + 1];

//
out vec4 FragColor;

vec3 getBaseColor() {
	vec3 color = u_baseColor;
#ifdef HAS_BASECOLOR_TEXTURE
	color *= pow(texture(u_baseColorTexture, v_texcoord).rgb, vec3(2.2));
#endif
	return color;
}

float getMetalness() {
	float val = u_metalness;
#ifdef HAS_METALNESS_TEXTURE
	val *= texture(u_metalnessTexture, v_texcoord).b;
// val *= texture(u_metalnessTexture, v_texcoord).r;
#endif
	return val;
}

float getRoughness() {
	float val = u_roughness;
#ifdef HAS_ROUGHNESS_TEXTURE
	val *= texture(u_roughnessTexture, v_texcoord).g;
// val *= texture(u_roughnessTexture, v_texcoord).r;
#endif
	return val;
}

float getAo() {
	float val = u_ao;
#ifdef HAS_AO_TEXTURE
	val *= texture(u_aoTexture, v_texcoord).r;
#endif
	return val;
}

vec3 getEmissive() {
	vec3 val = u_emissive;
#ifdef HAS_EMISSIVE_TEXTURE
	// val *= texture(u_emissiveTexture, v_texcoord).rgb;
	val *= pow(texture(u_emissiveTexture, v_texcoord).rgb, vec3(2.2));
#endif
	return val;
}

vec3 getNormal() {
#ifdef HAS_NORMAL_TEXTURE
	vec3 tangentNormal = texture(u_normalTexture, v_texcoord).xyz * 2.0 - 1.0;

	vec3 Q1 = dFdx(v_position);
	vec3 Q2 = dFdy(v_position);
	vec2 st1 = dFdx(v_texcoord);
	vec2 st2 = dFdy(v_texcoord);

	vec3 N = normalize(v_normal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
#else
	return normalize(v_normal);
#endif
}

vec3 getIrradiance(vec3 N) {
#ifdef HAS_IBL
	return texture(u_irradianceMap, N).rgb;
#else
	return vec3(0.03);
#endif
}

void main() {
	vec3 N = getNormal();
	vec3 V = normalize(u_Camera - v_position);
	vec3 R = reflect(-V, N);

	vec3 baseColor = getBaseColor();
	vec3 emissive = getEmissive();
	float metallic = getMetalness();
	float roughness = getRoughness();
	float ao = getAo();

	// calculate reflectance at normal incidence; if dia-electric (like plastic)
	// use F0 of 0.04 and if it's a metal, use the albedo color as F0 (metallic
	// workflow)
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, baseColor, metallic);

	// reflectance equation
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < 4; ++i) {
		// calculate per-light radiance
		vec3 L = normalize(lightPositions[i] - v_position);
		vec3 H = normalize(V + L);
		float distance = length(lightPositions[i] - v_position);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = lightColors[i] * attenuation;

		// Cook-Torrance BRDF
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) +
							0.0001;	 // + 0.0001 to prevent divide by zero
		vec3 specular = numerator / denominator;

		// kS is equal to Fresnel
		vec3 kS = F;
		// for energy conservation, the diffuse and specular light can't
		// be above 1.0 (unless the surface emits light); to preserve this
		// relationship the diffuse component (kD) should equal 1.0 - kS.
		vec3 kD = vec3(1.0) - kS;
		// multiply kD by the inverse metalness such that only non-metals
		// have diffuse lighting, or a linear blend if partly metal (pure metals
		// have no diffuse light).
		kD *= 1.0 - metallic;

		// scale light by NdotL
		float NdotL = max(dot(N, L), 0.0);

		// add to outgoing radiance Lo
		Lo += (kD * baseColor / PI + specular) * radiance *
			  NdotL;  // note that we already multiplied the BRDF by the Fresnel
					  // (kS) so we won't multiply by kS again
	}

	// ambient lighting (we now use IBL as the ambient term)
	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallic;

	vec3 irradiance = getIrradiance(N);
	vec3 diffuse = irradiance * baseColor;

#ifdef HAS_IBL
	// sample both the pre-filter map and the BRDF lut and combine them together
	// as per the Split-Sum approximation to get the IBL specular part.
	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor =
		textureLod(u_prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
	vec2 brdf = texture(u_brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
#else
	vec3 specular = vec3(0);
#endif

	vec3 ambient = (kD * diffuse + specular) * ao;

	vec3 color = ambient + Lo + emissive;

	// HDR tonemap and gamma correct
	color = toneMap_KhronosPbrNeutral(color);
	color = linearTosRGB(color);

	FragColor = vec4(color, 1.0);
}
