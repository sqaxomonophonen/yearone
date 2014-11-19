#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "a.h"
#include "text.h"

// ter_u24.c
extern int ter_u24n_bitmap_width;
extern int ter_u24n_bitmap_height;
extern int ter_u24n_n_variants;
extern int ter_u24n_n_meta;
extern int ter_u24n_meta[];
extern char ter_u24n_data[];

struct font* font_ter24;

static struct font _font_ter24;

void fonts_init()
{
	_font_ter24.bitmap_width = ter_u24n_bitmap_width;
	_font_ter24.bitmap_height = ter_u24n_bitmap_height;
	_font_ter24.n_variants = ter_u24n_n_variants;
	_font_ter24.n_meta = ter_u24n_n_meta;
	_font_ter24.meta = ter_u24n_meta;
	_font_ter24.data = ter_u24n_data;

	glGenTextures(1, &_font_ter24.texture); CHKGL;
	glBindTexture(GL_TEXTURE_2D, _font_ter24.texture); CHKGL;
	int level = 0;
	int border = 0;
	glTexImage2D(GL_TEXTURE_2D, level, 1, _font_ter24.bitmap_width, _font_ter24.bitmap_height, border, GL_RED, GL_UNSIGNED_BYTE, _font_ter24.data); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHKGL;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHKGL;

	font_ter24 = &_font_ter24;
}

void text_init(struct text* text)
{
	memset(text, 0, sizeof(*text));

	{
		#include "text.glsl.inc"
		shader_init(&text->shader, text_vert_src, text_frag_src);
		shader_use(&text->shader);
		text->a_position = glGetAttribLocation(text->shader.program, "a_position"); CHKGL;
		text->a_uv = glGetAttribLocation(text->shader.program, "a_uv"); CHKGL;
		text->u_color = glGetUniformLocation(text->shader.program, "u_color"); CHKGL;
		glUniform1i(glGetUniformLocation(text->shader.program, "u_texture"), 0); CHKGL;
	}
}

void text_set_window_dimensions(struct text* text, int width, int height)
{
	text->window_width = width;
	text->window_height = height;
}

void text_set_style(struct text* text, struct font* font, int variant, float color[4])
{
	text->current_font = font;
	text->current_variant = variant;
	for (int i = 0; i < 4; i++) text->current_color[i] = color[i];
}

void text_set_cursor(struct text* text, int cx, int cy)
{
	text->cx = cx;
	text->cy = cy;
}

void text_printf(struct text* text, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buffer[32768];
	int n = vsnprintf(buffer, 32767, fmt, args);
	va_end(args);
	if (n < 0) return;
}
