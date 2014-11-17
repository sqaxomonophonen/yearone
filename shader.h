#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>

struct shader {
	GLuint program;
};

void shader_init(struct shader*, const char* vertex, const char* fragment);
void shader_use(struct shader*);

#endif/*SHADER_H*/
