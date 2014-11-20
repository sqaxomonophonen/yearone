@vert
#version 130

attribute vec2 a_position;
attribute vec2 a_uv;

varying vec2 v_uv;

void main()
{
	v_uv = a_uv;
	gl_Position = vec4(a_position, 0, 1);
}


@frag
#version 130

varying vec2 v_uv;

uniform vec2 u_mu;
uniform vec4 u_color1;

void main(void)
{
	const float N = 3;
	vec4 color = vec4(0,0,0,0);
	for (float dy = -N; dy <= N; dy++) {
		for (float dx = -N; dx <= N; dx++) {
			vec2 uv = v_uv + vec2(dx,dy) * u_mu;
			float fx = 1 - fract(uv.x);
			float fx3 = fx*fx*fx;
			float thr = 0.25 + fx3 * 0.1;
			if (uv.y >= -thr && uv.y <= thr) {
				color += u_color1 * (0.6 + fx3 * 0.4);
				if (uv.x < 0.5) color += vec4(1,1,1,1) * (0.5-uv.x);
			}
		}
	}
	gl_FragColor = color / float(N*N*4);
}




