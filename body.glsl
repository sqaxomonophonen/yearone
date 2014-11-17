@vert
#version 130

attribute vec2 a_position;

varying vec2 v_position;

uniform vec2 u_offset;
uniform vec2 u_scale;

void main()
{
	v_position = a_position;
	gl_Position = vec4(a_position * u_scale + u_offset, 0, 1);
}


@frag
#version 130

varying vec2 v_position;
uniform float u_mu;
uniform vec2 u_light;
uniform vec3 u_color;

void main(void)
{
	const float N = 3;
	vec4 color = vec4(0,0,0,0);
	float tau = 1.5707963267948966;
	float ambient = 0.06;
	float bright = 1 - ambient;
	for (float dy = -N; dy <= N; dy++) {
		for (float dx = -N; dx <= N; dx++) {
			vec2 spos = v_position + vec2(dx*u_mu,dy*u_mu);
			float r = length(spos);
			if (r < 0.98) {
				vec3 lvec = vec3(u_light, 0);
				vec3 svec = vec3(normalize(spos) * sin(r*tau), cos(r*tau));
				float d = dot(lvec, svec);
				float x = d < 0 ? ambient : ambient + d * bright;
				color += vec4(u_color*x,1);
			}
		}
	}
	gl_FragColor = color / float(N*N*4);
}


