shader_type sky;
// A blue sky with clouds drifting across it, a pale haze toward the
// horizon and the sun where the arena's light comes from.

uniform vec3 top_color : source_color = vec3(0.2, 0.46, 0.9);
uniform vec3 horizon_color : source_color = vec3(0.72, 0.87, 0.98);
uniform vec3 ground_color : source_color = vec3(0.55, 0.6, 0.62);
uniform vec3 cloud_color : source_color = vec3(1.0, 1.0, 1.0);
uniform vec3 cloud_shade : source_color = vec3(0.76, 0.81, 0.9);
// A cloudy pattern that repeats without a seam.
uniform sampler2D clouds : repeat_enable, filter_linear_mipmap;
// How much of the sky is clear, from 0 (overcast) to 1 (cloudless).
uniform float clear = 0.5;
uniform float drift = 0.004;
// How bright the stars are: none by day.
uniform float stars = 0.0;

void sky() {
	vec3 colour = mix(horizon_color, top_color, pow(clamp(EYEDIR.y, 0.0, 1.0), 0.55));
	if (EYEDIR.y < 0.0) {
		colour = mix(horizon_color, ground_color, clamp(-EYEDIR.y * 6.0, 0.0, 1.0));
	} else {
		// The clouds lie on a flat layer overhead, so they bunch up and
		// shrink toward the horizon as real ones do.
		vec2 at = EYEDIR.xz / (EYEDIR.y + 0.16) * 0.16 + vec2(TIME * drift, TIME * drift * 0.4);
		float thick = texture(clouds, at).r * 0.65 + texture(clouds, at * 2.7 + vec2(0.37, 0.11) - TIME * drift * 0.5).r * 0.35;
		float cover = smoothstep(clear, clear + 0.16, thick);
		// Bright where a cloud is thick, greyer at its thin edges.
		vec3 cloud = mix(cloud_shade, cloud_color, smoothstep(clear + 0.04, clear + 0.3, thick));
		colour = mix(colour, cloud, cover * smoothstep(0.0, 0.2, EYEDIR.y));
	}
	if (stars > 0.0 && EYEDIR.y > 0.0) {
		// A scatter of points that stay put on the sky.
		vec3 cell = floor(EYEDIR * 170.0);
		float chance = fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
		colour += vec3(0.9, 0.95, 1.0) * stars * step(0.9965, chance) * smoothstep(0.0, 0.25, EYEDIR.y);
	}
	if (LIGHT0_ENABLED) {
		float toward = max(dot(EYEDIR, LIGHT0_DIRECTION), 0.0);
		colour += vec3(1.0, 0.95, 0.85) * (pow(toward, 900.0) * 2.0 + pow(toward, 24.0) * 0.18);
	}
	COLOR = colour;
}
