#ifndef __MUD_H__
#define __MUD_H__

#include <stdint.h>

// my useless data

int mud_open(const char* pathname);
void mud_readn(int fd, void* vbuf, size_t count);
void mud_close(int fd);

//int mud_load_png_palette(const char* path, uint8_t* palette);
int mud_load_png_paletted(const char* path, uint8_t** data, int* widthp, int* heightp);
//int mud_load_png_rgb(const char* path, uint8_t** data, int* widthp, int* heightp);


#endif//__MUD_H__
