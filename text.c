#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "a.h"
#include "text.h"
#include "utf8_decode.h"

// ter_u24.c
extern int ter_u24n_bitmap_width;
extern int ter_u24n_bitmap_height;
extern int ter_u24n_n_variants;
extern int ter_u24n_n_meta;
extern int ter_u24n_size;
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
	_font_ter24.size = ter_u24n_size;
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

static int* font_find_meta(struct font* font, int variant, int codepoint)
{
	int imin = 0;
	int imax = font->n_meta-1;
	while (imax >= imin) {
		int imid = (imin + imax) >> 1;
		int* meta = &font->meta[imid*6];
		if (meta[0] == variant && meta[1] == codepoint) {
			return meta;
		} else if (meta[0] < variant || (meta[0] == variant && meta[1] < codepoint)) {
			imin = imid + 1;
		} else {
			imax = imid - 1;
		}
	}
	return NULL;
}

#define FLOATS_PER_VERTEX (8)

void text_init(struct text* text)
{
	memset(text, 0, sizeof(*text));

	{
		#include "text.glsl.inc"
		shader_init(&text->shader, text_vert_src, text_frag_src);
		shader_use(&text->shader);
		text->a_position = glGetAttribLocation(text->shader.program, "a_position"); CHKGL;
		text->a_uv = glGetAttribLocation(text->shader.program, "a_uv"); CHKGL;
		text->a_color = glGetAttribLocation(text->shader.program, "a_color"); CHKGL;
		glUniform1i(glGetUniformLocation(text->shader.program, "u_texture"), 0); CHKGL;
	}

	text->n_quads = 0;
	text->max_quads = 4096;

	{
		glGenBuffers(1, &text->vertex_buffer); CHKGL;
		glBindBuffer(GL_ARRAY_BUFFER, text->vertex_buffer); CHKGL;
		size_t sz = sizeof(float) * FLOATS_PER_VERTEX * 4 * text->max_quads;
		AN(text->vertex_data = malloc(sz));
		glBufferData(GL_ARRAY_BUFFER, sz, text->vertex_data, GL_STREAM_DRAW); CHKGL;
	}

	{
		glGenBuffers(1, &text->index_buffer); CHKGL;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text->index_buffer); CHKGL;
		int n = text->max_quads * 4;
		size_t sz = sizeof(int16_t) * n;
		uint16_t* index_data;
		AN(index_data = malloc(sz));
		for (int i = 0; i < n; i++) index_data[i] = i;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz, index_data, GL_STATIC_DRAW); CHKGL;
		free(index_data);
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
	text->cx0 = cx;
	text->cx = cx;
	text->cy = cy;
}

void text_emit_quad(struct text* text, int u, int v, int w, int h)
{
	if (text->n_quads >= text->max_quads) text_flush(text);
	ASSERT(text->n_quads < text->max_quads);
	int i = 0;
	float* p = &text->vertex_data[text->n_quads * FLOATS_PER_VERTEX * 4];
	for (int y = 0; y < 2; y++) {
		for (int mx = 0; mx < 2; mx++) {
			int x = mx^y;
			// x/y
			p[i++] = ((float)(text->cx + w*x) / (float)text->window_width) * 2.0 - 1.0;
			p[i++] = ((float)(text->cy + h*y) / (float)text->window_height) * -2.0 + 1.0;

			// u/v
			p[i++] = (float)(u + w*x) / (float)text->current_font->bitmap_width;
			p[i++] = (float)(v + h*y) / (float)text->current_font->bitmap_height;

			// color
			for (int c = 0; c < 4; c++) p[i++] = text->current_color[c];
		}
	}
	text->n_quads++;
}

void text_put_codepoint(struct text* text, int codepoint)
{
	if (codepoint == '\r') {
		text->cx = text->cx0;
	} else if (codepoint == '\n') {
		text->cx = text->cx0;
		text->cy += text->current_font->size;
	} else {
		int* meta = font_find_meta(text->current_font, text->current_variant, codepoint);
		if (meta == NULL) {
			meta = font_find_meta(text->current_font, text->current_variant, 0xfffd);
			if (meta == NULL) {
				return;
			}
		}
		AN(meta);
		text_emit_quad(text, meta[2], meta[3], meta[4], meta[5]);
		text->cx += meta[4];
	}
}

void text_printf(struct text* text, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buffer[32768];
	int n = vsnprintf(buffer, 32767, fmt, args);
	va_end(args);
	if (n <= 0) return;
	char* p = buffer;
	while (n > 0) text_put_codepoint(text, utf8_decode(&p, &n));
}

void text_flush(struct text* text)
{
	shader_use(&text->shader);
	glEnable(GL_TEXTURE_2D); CHKGL;
	glBindTexture(GL_TEXTURE_2D, text->current_font->texture); CHKGL;

	glBindBuffer(GL_ARRAY_BUFFER, text->vertex_buffer); CHKGL;
	glBufferSubData(GL_ARRAY_BUFFER, 0, text->n_quads * sizeof(float) * FLOATS_PER_VERTEX * 4, text->vertex_data); CHKGL;

	glEnableVertexAttribArray(text->a_position); CHKGL;
	glEnableVertexAttribArray(text->a_uv); CHKGL;
	glEnableVertexAttribArray(text->a_color); CHKGL;

	glVertexAttribPointer(text->a_position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_VERTEX, 0); CHKGL;
	glVertexAttribPointer(text->a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_VERTEX, (char*)(sizeof(float)*2)); CHKGL;
	glVertexAttribPointer(text->a_color, 4, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_VERTEX, (char*)(sizeof(float)*4)); CHKGL;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text->index_buffer); CHKGL;
	glDrawElements(GL_QUADS, text->n_quads * 4, GL_UNSIGNED_SHORT, NULL); CHKGL;

	glDisableVertexAttribArray(text->a_color); CHKGL;
	glDisableVertexAttribArray(text->a_uv); CHKGL;
	glDisableVertexAttribArray(text->a_position); CHKGL;

	glDisable(GL_TEXTURE_2D);

	text->n_quads = 0;
}
