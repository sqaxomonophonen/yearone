@vert
#version 130

attribute vec2 a_position;
attribute vec2 a_uv;
attribute vec4 a_color;

varying vec2 v_uv;
varying vec4 v_color;

void main()
{
	v_uv = a_uv;
	v_color = a_color;
	gl_Position = vec4(a_position, 0, 1);
}

@frag
#version 130

uniform sampler2D u_texture;

varying vec2 v_uv;
varying vec4 v_color;

void main(void)
{
	float sample = texture2D(u_texture, v_uv).r;
	if (sample < 0.5) discard;
	gl_FragColor = v_color;
}

