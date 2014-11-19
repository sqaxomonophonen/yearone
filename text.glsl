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

uniform vec4 u_color;
uniform sampler2D u_texture;

varying vec2 v_uv;

void main(void)
{
	float sample = texture2D(u_texture, v_uv).r;
	if (sample < 0.5) discard;
	gl_FragColor = u_color;
}

