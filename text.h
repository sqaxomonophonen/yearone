#ifndef TEXT_H
#define TEXT_H

#include <GL/glew.h>

#include "shader.h"

struct font {
	GLuint texture;
	int bitmap_width;
	int bitmap_height;
	int n_variants;
	int n_meta;
	int* meta;
	char* data;
};

extern struct font* font_ter24;

void fonts_init();

struct text {
	struct shader shader;
	GLuint a_position;
	GLuint a_uv;
	GLuint u_color;
	int window_width;
	int window_height;

	struct font* current_font;
	int current_variant;
	float current_color[4];
	int cx,cy;
};

void text_init(struct text* text);
void text_set_window_dimensions(struct text* text, int width, int height);
void text_set_style(struct text* text, struct font* font, int variant, float color[4]);
void text_set_cursor(struct text* text, int cx, int cy);
void text_printf(struct text* text, const char* fmt, ...) __attribute__((format (printf, 2, 3)));


#endif/*TEXT_H*/
