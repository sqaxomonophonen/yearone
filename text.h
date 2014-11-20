#ifndef TEXT_H
#define TEXT_H

#include <GL/glew.h>

#include "shader.h"

struct font {
	GLuint texture;
	int bitmap_width;
	int bitmap_height;
	int size;
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
	GLuint a_color;
	int window_width;
	int window_height;

	struct font* current_font;
	int current_variant;
	float current_color[4];
	int cx0,cx,cy;

	GLuint vertex_buffer;
	float* vertex_data;
	GLuint index_buffer;
	int max_quads;
	int n_quads;
};

void text_init(struct text* text);
void text_set_window_dimensions(struct text* text, int width, int height);
void text_set_font(struct text* text, struct font* font);
void text_set_variant(struct text* text, int variant);
void text_set_color(struct text* text, float color[4]);
void text_set_color3f(struct text* text, float r, float g, float b);
void text_set_color4f(struct text* text, float r, float g, float b, float a);
void text_set_cursor(struct text* text, int cx, int cy);
void text_printf(struct text* text, const char* fmt, ...) __attribute__((format (printf, 2, 3)));
void text_flush(struct text* text);


#endif/*TEXT_H*/
