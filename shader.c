#include <stdlib.h>
#include "shader.h"
#include "a.h"

static GLuint create_shader(GLenum type, const char* src)
{
	GLuint shader = glCreateShader(type); CHKGL;
	glShaderSource(shader, 1, &src, 0);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint msglen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &msglen);
		GLchar* msg = (GLchar*) malloc(msglen + 1);
		glGetShaderInfoLog(shader, msglen, NULL, msg);
		const char* stype = type == GL_VERTEX_SHADER ? "vertex" : type == GL_FRAGMENT_SHADER ? "fragment" : "waaaat";
		arghf("%s shader error: %s -- source:\n%s", stype, msg, src);
	}
	return shader;
}

void shader_init(struct shader* s, const char* vertex, const char* fragment)
{
	GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex);
	GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment);

	s->program = glCreateProgram(); CHKGL;

	glAttachShader(s->program, vertex_shader);
	glAttachShader(s->program, fragment_shader);

	// glBindAttribLocation?!

	glLinkProgram(s->program);

	GLint status;
	glGetProgramiv(s->program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint msglen;
		glGetProgramiv(s->program, GL_INFO_LOG_LENGTH, &msglen);
		GLchar* msg = (GLchar*) malloc(msglen + 1);
		glGetProgramInfoLog(s->program, msglen, NULL, msg);
		arghf("shader link error: %s", msg);
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
}

void shader_use(struct shader* s)
{
	glUseProgram(s->program);
}

