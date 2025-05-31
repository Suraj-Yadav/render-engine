in vec3 v_position;
//
uniform sampler2D u_textureData;
//
out vec4 FragColor;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
	vec2 uv = vec2(atan(-v.y, v.x), asin(v.z));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main() {
	vec2 uv = SampleSphericalMap(normalize(v_position.xyz));
	FragColor.rgb = texture(u_textureData, uv).rgb;
	FragColor.a = 1.0;
}
