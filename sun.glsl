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

void main(void)
{
	const float N = 3;
	vec4 color = vec4(0,0,0,0);
	for (float dy = -N; dy <= N; dy++) {
		for (float dx = -N; dx <= N; dx++) {
			vec2 spos = v_position + vec2(dx*u_mu,dy*u_mu);
			float r = dot(spos, spos);
			if (r < 0.4) {
				color += vec4(1,1,1-r,1);
			} else if (r < 0.95) {
				color += vec4(0.8-r,0.6-r,0.5-r,1);
			}
		}
	}
	gl_FragColor = color / float(N*N*4);
}



