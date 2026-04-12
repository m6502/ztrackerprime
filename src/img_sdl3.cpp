#include "img.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Surface *zt_load_png_rgba32(FILE *fp) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    const png_uint_32 width = png_get_image_width(png, info);
    const png_uint_32 height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    png_read_update_info(png, info);

    const png_size_t rowbytes = png_get_rowbytes(png, info);
    png_bytep pixels = (png_bytep)malloc(rowbytes * height);
    if (!pixels) {
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!rows) {
        free(pixels);
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    for (png_uint_32 y = 0; y < height; ++y) {
        rows[y] = pixels + y * rowbytes;
    }

    png_read_image(png, rows);
    png_read_end(png, NULL);

    SDL_Surface *surface = SDL_CreateSurface((int)width, (int)height, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        free(rows);
        free(pixels);
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    if (SDL_LockSurface(surface)) {
        for (png_uint_32 y = 0; y < height; ++y) {
            memcpy((Uint8 *)surface->pixels + y * surface->pitch, rows[y], rowbytes);
        }
        SDL_UnlockSurface(surface);
    }

    free(rows);
    free(pixels);
    png_destroy_read_struct(&png, &info, NULL);
    return surface;
}

SDL_Surface *SDL_LoadPNG(const char *file) {
    if (!file) {
        return NULL;
    }

    FILE *fp = fopen(file, "rb");
    if (!fp) {
        return NULL;
    }

    unsigned char sig[8];
    if (fread(sig, 1, sizeof(sig), fp) != sizeof(sig) || png_sig_cmp(sig, 0, sizeof(sig)) != 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    SDL_Surface *surface = zt_load_png_rgba32(fp);
    fclose(fp);
    return surface;
}

SDL_Surface *zoomSurface(SDL_Surface *src, double zoomx, double zoomy, int smooth) {
    if (!src || zoomx <= 0.0 || zoomy <= 0.0) {
        return NULL;
    }

    int dst_w = (int)((double)src->w * zoomx);
    int dst_h = (int)((double)src->h * zoomy);
    if (dst_w < 1) dst_w = 1;
    if (dst_h < 1) dst_h = 1;

    SDL_Surface *dst = SDL_CreateSurface(dst_w, dst_h, src->format);
    if (!dst) {
        return NULL;
    }

    const SDL_ScaleMode mode = smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
    if (!SDL_BlitSurfaceScaled(src, NULL, dst, NULL, mode)) {
        SDL_DestroySurface(dst);
        return NULL;
    }

    return dst;
}
