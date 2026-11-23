#define _GNU_SOURCE
#include "gui_img.h"
#include "tunables.h"
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <jpeglib.h>   /* after stdio.h and stdlib.h: it needs FILE and size_t */
#include <webp/decode.h>

bool img_picture_name(const char *name) {
    if (!name || name[0] == '.') return false;
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return false;
    return !strcasecmp(dot, ".png") || !strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg") || !strcasecmp(dot, ".webp");
}

static bool size_ok(uint64_t w, uint64_t h) { return w > 0 && h > 0 && w <= 65535 && h <= 65535 && w * h <= IMG_PIXELS_MAX; }

static uint8_t *read_file(const char *path, size_t *n) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > IMG_FILE_MAX) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    uint8_t *d = malloc((size_t)st.st_size);
    size_t got = d ? fread(d, 1, (size_t)st.st_size, f) : 0;
    fclose(f);
    if (got != (size_t)st.st_size) { free(d); return NULL; }
    *n = got;
    return d;
}

/* ---- JPEG ---- */

struct jfail { struct jpeg_error_mgr mgr; jmp_buf back; };
static void jquiet(j_common_ptr c) { (void)c; }   /* a damaged file is not a message on the terminal */
static void jfail(j_common_ptr c) { longjmp(((struct jfail *)c->err)->back, 1); }

static cairo_surface_t *decode_jpeg(const uint8_t *data, size_t size, int want) {
    struct jpeg_decompress_struct cinfo;
    struct jfail fail;
    cairo_surface_t *volatile surf = NULL;
    cinfo.err = jpeg_std_error(&fail.mgr);
    fail.mgr.error_exit = jfail;
    fail.mgr.output_message = jquiet;
    if (setjmp(fail.back)) {
        jpeg_destroy_decompress(&cinfo);
        if (surf) cairo_surface_destroy(surf);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, size);
    jpeg_read_header(&cinfo, TRUE);
    if (!size_ok(cinfo.image_width, cinfo.image_height)) longjmp(fail.back, 1);
    cinfo.out_color_space = JCS_EXT_BGRX;   /* what cairo's ARGB32 is in memory, with the unused byte 0xFF: opaque */
    if (want > 0) {   /* the smallest picture that is still big enough: 1/2, 1/4 or 1/8 of it, made by the decoder itself */
        unsigned short_side = cinfo.image_width < cinfo.image_height ? cinfo.image_width : cinfo.image_height;
        unsigned d = 1;
        while (d < 8 && short_side / (d * 2) >= (unsigned)want) d *= 2;
        cinfo.scale_num = 1;
        cinfo.scale_denom = d;
    }
    jpeg_start_decompress(&cinfo);
    if (cinfo.output_components != 4) longjmp(fail.back, 1);
    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, (int)cinfo.output_width, (int)cinfo.output_height);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) longjmp(fail.back, 1);
    unsigned char *px = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = px + (size_t)cinfo.output_scanline * (size_t)stride;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    cairo_surface_mark_dirty(surf);
    return surf;
}

/* The orientation (1 to 8) that a camera wrote in the EXIF of a JPEG; 1 (as it is) if there is none or it is damaged. Every read is checked. */
static unsigned exif_orientation(const uint8_t *d, size_t n) {
    size_t at = 2;   /* after FF D8 */
    while (at + 4 <= n && d[at] == 0xFF) {
        unsigned marker = d[at + 1], len = (unsigned)(d[at + 2] << 8 | d[at + 3]);
        if (marker == 0xDA || marker == 0xD9 || len < 2 || at + 2 + len > n) break;   /* the picture itself starts: no more headers */
        const uint8_t *seg = d + at + 4;
        size_t sn = len - 2;
        if (marker == 0xE1 && sn >= 14 && memcmp(seg, "Exif\0\0", 6) == 0) {
            const uint8_t *tiff = seg + 6;
            size_t tn = sn - 6;
            bool le = tiff[0] == 'I' && tiff[1] == 'I';
            if (!le && !(tiff[0] == 'M' && tiff[1] == 'M')) return 1;
#define R16(p) (le ? (unsigned)((p)[0] | (p)[1] << 8) : (unsigned)((p)[0] << 8 | (p)[1]))
#define R32(p) (le ? (uint32_t)(p)[0] | (uint32_t)(p)[1] << 8 | (uint32_t)(p)[2] << 16 | (uint32_t)(p)[3] << 24 \
                    : (uint32_t)(p)[0] << 24 | (uint32_t)(p)[1] << 16 | (uint32_t)(p)[2] << 8 | (uint32_t)(p)[3])
            if (R16(tiff + 2) != 42) return 1;
            uint32_t ifd = R32(tiff + 4);
            if (ifd > tn || tn - ifd < 2) return 1;
            unsigned count = R16(tiff + ifd);
            for (unsigned i = 0; i < count; i++) {
                size_t e = (size_t)ifd + 2 + (size_t)i * 12;
                if (e + 12 > tn) return 1;
                if (R16(tiff + e) == 0x0112) {
                    unsigned o = R16(tiff + e + 8);
                    return o >= 1 && o <= 8 ? o : 1;
                }
            }
            return 1;
        }
        at += 2 + len;
    }
    return 1;
}

/* The surface turned as the camera was held (2 mirrored, 3 upside down, 4 mirrored the other way, 5 to 8 turned a quarter, some of them
 * mirrored too): a new one, and the old one is gone. The same one if there is nothing to do or no memory. */
static cairo_surface_t *orient(cairo_surface_t *s, unsigned o) {
    if (o <= 1 || o > 8) return s;
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int dw = o >= 5 ? h : w, dh = o >= 5 ? w : h;
    cairo_surface_t *r = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dw, dh);
    if (cairo_surface_status(r) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(r); return s; }
    cairo_surface_flush(s);
    const unsigned char *sp = cairo_image_surface_get_data(s);
    unsigned char *dp = cairo_image_surface_get_data(r);
    int ss = cairo_image_surface_get_stride(s), ds = cairo_image_surface_get_stride(r);
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int sx, sy;
            switch (o) {
            case 2: sx = w - 1 - x; sy = y; break;
            case 3: sx = w - 1 - x; sy = h - 1 - y; break;
            case 4: sx = x; sy = h - 1 - y; break;
            case 5: sx = y; sy = x; break;
            case 6: sx = y; sy = h - 1 - x; break;
            case 7: sx = w - 1 - y; sy = h - 1 - x; break;
            default: sx = w - 1 - y; sy = x; break;   /* 8 */
            }
            *(uint32_t *)(dp + (size_t)y * (size_t)ds + (size_t)x * 4) = *(const uint32_t *)(sp + (size_t)sy * (size_t)ss + (size_t)sx * 4);
        }
    }
    cairo_surface_mark_dirty(r);
    cairo_surface_destroy(s);
    return r;
}

/* ---- WebP ---- */

static cairo_surface_t *decode_webp(const uint8_t *data, size_t size, int want) {
    WebPDecoderConfig cfg;
    if (!WebPInitDecoderConfig(&cfg) || WebPGetFeatures(data, size, &cfg.input) != VP8_STATUS_OK) return NULL;
    int w = cfg.input.width, h = cfg.input.height;
    if (!size_ok((uint64_t)w, (uint64_t)h)) return NULL;
    int short_side = w < h ? w : h;
    if (want > 0 && short_side > want * 2) {   /* scaled while it is decoded: never less than `want` on the short side */
        cfg.options.use_scaling = 1;
        cfg.options.scaled_width = (int)((int64_t)w * want / short_side);
        cfg.options.scaled_height = (int)((int64_t)h * want / short_side);
        if (cfg.options.scaled_width < 1) cfg.options.scaled_width = 1;
        if (cfg.options.scaled_height < 1) cfg.options.scaled_height = 1;
        w = cfg.options.scaled_width;
        h = cfg.options.scaled_height;
    }
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(surf); return NULL; }
    int stride = cairo_image_surface_get_stride(surf);
    cfg.output.colorspace = MODE_bgrA;   /* B, G, R, premultiplied A: cairo's ARGB32 on a little-endian machine */
    cfg.output.is_external_memory = 1;
    cfg.output.u.RGBA.rgba = cairo_image_surface_get_data(surf);
    cfg.output.u.RGBA.stride = stride;
    cfg.output.u.RGBA.size = (size_t)stride * (size_t)h;
    cairo_surface_flush(surf);
    VP8StatusCode rc = WebPDecode(data, size, &cfg);
    WebPFreeDecBuffer(&cfg.output);
    if (rc != VP8_STATUS_OK) { cairo_surface_destroy(surf); return NULL; }
    cairo_surface_mark_dirty(surf);
    return surf;
}

/* ---- any ---- */

static cairo_surface_t *finish_png(cairo_surface_t *s) {
    if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS || cairo_image_surface_get_width(s) < 1 || cairo_image_surface_get_height(s) < 1 ||
        !size_ok((uint64_t)cairo_image_surface_get_width(s), (uint64_t)cairo_image_surface_get_height(s))) {
        cairo_surface_destroy(s);
        return NULL;
    }
    return s;
}

cairo_surface_t *img_load(const char *path, int want) {
    size_t n = 0;
    uint8_t *d = read_file(path, &n);
    if (!d) return NULL;
    cairo_surface_t *s = NULL;
    if (n >= 8 && memcmp(d, "\x89PNG\r\n\x1a\n", 8) == 0) s = finish_png(cairo_image_surface_create_from_png(path));
    else if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) {
        s = decode_jpeg(d, n, want);
        if (s) s = orient(s, exif_orientation(d, n));
    } else if (n >= 12 && memcmp(d, "RIFF", 4) == 0 && memcmp(d + 8, "WEBP", 4) == 0) s = decode_webp(d, n, want);
    free(d);
    return s;
}
