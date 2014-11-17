#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <png.h>

#include "mud.h"
#include "a.h"

int mud_open(const char* pathname)
{
	// TODO patch pathname to MUD directory?
	int fd = open(pathname, O_RDONLY);
	if(fd == -1) {
		arghf("open(%s): %s", pathname, strerror(errno));
	}
	return fd;
}

void mud_readn(int fd, void* vbuf, size_t n)
{
	char* buf = (char*) vbuf;
	while(n > 0) {
		ssize_t n_read = read((int)fd, buf, n);
		if(n_read == -1) {
			if(errno == EINTR) continue;
			arghf("read: %s", strerror(errno));
		}
		n -= n_read;
		buf += n_read;
	}
}


void mud_close(int fd)
{
	int ret = close((int) fd);
	if(ret == -1) {
		arghf("close: %s", strerror(errno));
	}
}

static void user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
	arghf("libpng error - %s", error_msg);
}

static void user_warning_fn(png_structp png_ptr, png_const_charp warning_msg)
{
	arghf("libpng warning (promoted to error) - %s", warning_msg);
}

static void user_read_data_fn(png_structp png_ptr, png_bytep dest, png_size_t length)
{
	int fd = *((int*) png_get_io_ptr(png_ptr));
	mud_readn(fd, dest, length);
}

struct png_common {
	int fd;
	png_structp png_ptr;
	png_infop info_ptr;
	int width;
	int height;
	int channels;
	int bit_depth;
	int color_type;
	int rowbytes;
};

static void mud_load_png_common(const char* rel, int* widthp, int* heightp, struct png_common* pc)
{
	pc->fd = mud_open(rel);

	png_byte header[8];
	mud_readn(pc->fd, header, 8);
	if (png_sig_cmp(header, 0, 8) != 0) {
		arghf("mud_load_png_rgba: %s is not a PNG\n", rel);
	}

	pc->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)0, user_error_fn, user_warning_fn);
	if (pc->png_ptr == NULL) {
		arghf("png_create_read_struct failed for '%s'", rel);
	}

	pc->info_ptr = png_create_info_struct(pc->png_ptr);
	if (pc->info_ptr == NULL) {
		arghf("png_create_info_struct failed for '%s'", rel);
	}

	png_set_sig_bytes(pc->png_ptr, 8);
	png_set_read_fn(pc->png_ptr, &pc->fd, user_read_data_fn);
	png_read_info(pc->png_ptr, pc->info_ptr);

	pc->width = png_get_image_width(pc->png_ptr, pc->info_ptr);
	pc->height = png_get_image_height(pc->png_ptr, pc->info_ptr);

	if (widthp != NULL) *widthp = pc->width;
	if (heightp != NULL) *heightp = pc->height;

	pc->channels = png_get_channels(pc->png_ptr, pc->info_ptr);
	pc->bit_depth = png_get_bit_depth(pc->png_ptr, pc->info_ptr);
	pc->color_type = png_get_color_type(pc->png_ptr, pc->info_ptr);
	pc->rowbytes = png_get_rowbytes(pc->png_ptr, pc->info_ptr);
}

#if 0
int mud_load_png_palette(const char* path, uint8_t* palette)
{
	AN(palette);

	struct png_common pc;
	mud_load_png_common(path, NULL, NULL, &pc);

	if (pc.bit_depth != 8) {
		arghf("%s: bit depth != 8", path);
	}

	if (pc.channels != 1 || pc.color_type != PNG_COLOR_TYPE_PALETTE) {
		arghf("%s: not paletted", path);
	}

	png_color* pp;
	int num_palette = 0;
	png_get_PLTE(pc.png_ptr, pc.info_ptr, &pp, &num_palette);
	if (num_palette != 256) {
		arghf("%s: not a 256 palette file (got %d)", path, num_palette);
	}
	for (int i = 0; i < 256; i++) {
		palette[i*3] = pp[i].red;
		palette[i*3+1] = pp[i].green;
		palette[i*3+2] = pp[i].blue;
	}

	// FIXME clean up?
	mud_close(pc.fd);

	return 0;
}
#endif

int mud_load_png_paletted(const char* path, uint8_t** data, int* widthp, int* heightp)
{
	struct png_common pc;
	mud_load_png_common(path, widthp, heightp, &pc);

	if (pc.bit_depth != 8) {
		arghf("%s: bit depth != 8", path);
	}

	if (pc.channels != 1 || pc.color_type != PNG_COLOR_TYPE_PALETTE) {
		arghf("%s: not paletted", path);
	}

	if (data != NULL) {
		*data = malloc(pc.width * pc.height);

		png_bytep *row_pointers = (png_bytep*) malloc(pc.height * sizeof(png_bytep*));
		for(int i = 0; i < pc.height; i++) {
			row_pointers[i] = ((png_bytep) *data) + pc.rowbytes * i;
		}
		png_read_image(pc.png_ptr, row_pointers);
		free(row_pointers);
	}

	// FIXME clean up?
	mud_close(pc.fd);

	return 0;
}

#if 0
int mud_load_png_rgb(const char* path, uint8_t** data, int* widthp, int* heightp)
{
	struct png_common pc;
	mud_load_png_common(path, widthp, heightp, &pc);

	if (pc.bit_depth != 8) {
		arghf("%s: bit depth != 8", path);
	}

	if (pc.channels != 3 || pc.color_type != PNG_COLOR_TYPE_RGB) {
		arghf("%s: not RGB", path);
	}

	if(data != NULL) {
		*data = malloc(pc.width * pc.height * 3);

		png_bytep *row_pointers = (png_bytep*) malloc(pc.height * sizeof(png_bytep*));
		for(int i = 0; i < pc.height; i++) {
			row_pointers[i] = ((png_bytep) *data) + pc.rowbytes * i;
		}
		png_read_image(pc.png_ptr, row_pointers);
		free(row_pointers);
	}

	mud_close(pc.fd);

	return 0;
}

void mud_load_png_rgba(const char* rel, void** data, int* widthp, int* heightp)
{
	int fd = mud_open(rel);

	png_byte header[8];
	mud_readn(fd, header, 8);
	if(png_sig_cmp(header, 0, 8) != 0) {
		arghf("mud_load_png_rgba: %s is not a PNG\n", rel);
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)0, user_error_fn, user_warning_fn);
	if(png_ptr == NULL) {
		arghf("png_create_read_struct failed for '%s'", rel);
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(info_ptr == NULL) {
		arghf("png_create_info_struct failed for '%s'", rel);
	}

	png_set_sig_bytes(png_ptr, 8);
	png_set_read_fn(png_ptr, &fd, user_read_data_fn);
	png_read_info(png_ptr, info_ptr);

	int width = png_get_image_width(png_ptr, info_ptr);
	int height = png_get_image_height(png_ptr, info_ptr);

	if(widthp != NULL) *widthp = width;
	if(heightp != NULL) *heightp = height;

	int channels = png_get_channels(png_ptr, info_ptr);
	int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	int color_type = png_get_color_type(png_ptr, info_ptr);
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	if(bit_depth != 8) {
		arghf("%s: bit depth != 8", rel);
	}

	if(channels != 4 || color_type != PNG_COLOR_TYPE_RGBA) {
		arghf("%s: not RGBA", rel);
	}

	if(data != NULL) {
		*data = malloc(width * height * 4);

		png_bytep *row_pointers = (png_bytep*) malloc(height * sizeof(png_bytep*));
		for(int i = 0; i < height; i++) {
			row_pointers[i] = ((png_bytep) *data) + rowbytes * i;
		}
		png_read_image(png_ptr, row_pointers);
		free(row_pointers);
	}

	// FIXME clean up?
	mud_close(fd);
}

#endif
