#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h> 
#include <linux/videodev2.h>
#include <linux/fb.h>

#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include "fb.h"

#define FB_ASSERT assert(fb_fd > 0 && fb_addr && fb_newbuf)

int fb_width = 0, fb_height = 0, fb_bpp = 0;

static int fb_fd = 0;
static char *fb_addr = NULL;
static int fb_size = 0;
static int fb_xoffset = 0, fb_xsize = 0;
static char *fb_oldbuf = NULL, *fb_newbuf = NULL;
struct fb_var_screeninfo fb_vinfo;

static void init_font(void);
int fb_init(const char *path) {
	size_t sz;

	assert(fb_fd == 0 && fb_addr == NULL);
	
	fb_fd = open(path, O_RDWR);
	if(fb_fd < 0) {
		pprintf("open framebuffer device failed");
		return FB_ERR;
	}

	if(ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		pprintf("ioctl FBIOGET_VSCREENINFO failed");
		close(fb_fd);
		return FB_ERR;
	}

	fb_width = fb_vinfo.xres;
	fb_xoffset = fb_vinfo.xres_virtual * fb_vinfo.bits_per_pixel / 8;
	fb_xsize = fb_vinfo.xres * fb_vinfo.bits_per_pixel / 8;
	fb_height = fb_vinfo.yres;
	fb_bpp = fb_vinfo.bits_per_pixel;

	sz = fb_xsize * fb_height;
	fb_newbuf = (char*) malloc(sz);
	if(fb_newbuf == NULL) {
		pprintf("malloc fb_newbuf failed");
		close(fb_fd);
		return FB_ERR;
	}
	memset(fb_newbuf, 0, sz);
	dprintf("newbuf size is %.3lfMB\n", sz / 1024.0f / 1024.0f);

	fb_size = fb_vinfo.xres_virtual * fb_vinfo.yres_virtual * fb_bpp / 8;
	fb_addr = (char*) mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if(fb_addr == MAP_FAILED) {
		pprintf("mmap failed");
		close(fb_fd);
		return FB_ERR;
	}

#	define vcolor(x) fb_vinfo.x.offset, fb_vinfo.x.length, fb_vinfo.x.msb_right
	eprintf("size: %dx%d, bpp: %d, mmap: %p, red(%u,%u,%u), green(%u,%u,%u), blue(%u,%u,%u), transp(%u,%u,%u)\n", fb_width, fb_height, fb_bpp, fb_addr, vcolor(red), vcolor(green), vcolor(blue), vcolor(transp));
#	undef vcolor

	init_font();

	return FB_OK;
}

int fb_color(int red, int green, int blue) {
	int mask = (1 << fb_vinfo.red.length) - 1;

	red &= mask;
	green &= mask;
	blue &= mask;

	return (red << fb_vinfo.red.offset) | (green << fb_vinfo.green.offset) | (blue << fb_vinfo.blue.offset) | (fb_vinfo.transp.length ? (0xff << fb_vinfo.transp.offset) : 0);
}

int fb_color_add(int color, int add) {
	int mask = (1 << fb_vinfo.red.length) - 1;
	int red, green, blue;

	red = (color >> fb_vinfo.red.offset) & mask;
	green = (color >> fb_vinfo.green.offset) & mask;
	blue = (color >> fb_vinfo.blue.offset) & mask;

	add &= mask;

	red += add;
	green += add;
	blue += add;

	return (red << fb_vinfo.red.offset) | (green << fb_vinfo.green.offset) | (blue << fb_vinfo.blue.offset) | (fb_vinfo.transp.length ? (0xff << fb_vinfo.transp.offset) : 0);
}

static void free_font(void);
int fb_free(void) {
	FB_ASSERT;
	
	free_font();
	
	free(fb_newbuf);
	fb_newbuf = NULL;
	
	munmap(fb_addr, fb_size);
	fb_addr = NULL;
	fb_size = 0;

	close(fb_fd);
	fb_fd = 0;

	return FB_OK;
}

void fb_sync(void) {
	char *p, *p2;
	int i;
	
	p = fb_newbuf;
	p2 = fb_addr;
	for(i = 0; i < fb_height; i ++) {
		memcpy(p2, p, fb_xsize);
		
		p += fb_xsize;
		p2 += fb_xoffset;
	}
}

int fb_save(void) {
	char *p, *p2;
	int i;
	size_t sz;

	FB_ASSERT;

	if(fb_oldbuf) free(fb_oldbuf);

	sz = fb_xsize * fb_height;
	fb_oldbuf = (char*) malloc(sz);
	if(fb_oldbuf == NULL) return FB_ERR;

	dprintf("oldbuf size is %.3lfMB\n", sz / 1024.0f / 1024.0f);

	p = fb_oldbuf;
	p2 = fb_addr;
	for(i = 0; i < fb_height; i ++) {
		memcpy(p, p2, fb_xsize);
		
		p += fb_xsize;
		p2 += fb_xoffset;
	}
	
	return FB_OK;
}

int fb_restore(void) {
	char *p, *p2;
	int i;

	FB_ASSERT;
	if(fb_oldbuf == NULL) return FB_ERR;

	p = fb_oldbuf;
	p2 = fb_addr;
	for(i = 0; i < fb_height; i ++) {
		memcpy(p2, p, fb_xsize);
		
		p += fb_xsize;
		p2 += fb_xoffset;
	}
	
	free(fb_oldbuf);
	fb_oldbuf = NULL;

	return FB_OK;
}

#include "font_10x18.h"
static unsigned char *font_data = NULL;

static void init_font(void) {
	unsigned char* bits;
	size_t sz;

	if(font_data) return;

	sz = font.width * font.height;
	bits = malloc(sz);
	font_data = bits;

	dprintf("font_data size is %.3lfKB\n", sz / 1024.0f);

	unsigned char data;
    unsigned char* in = font.rundata;
    while((data = *in++)) {
        memset(bits, (data & 0x80) ? 255 : 0, data & 0x7f);
        bits += (data & 0x7f);
    }
}

static void free_font(void) {
	if(font_data) {
		free(font_data);
		font_data = NULL;
	}
}

static bool outside(int x, int y) {
    return x < 0 || x >= fb_width || y < 0 || y >= fb_height;
}

static void text_blend(unsigned char* src_p, int src_row_bytes, unsigned char* dst_p, int dst_row_bytes, int width, int height, int color, int size) {
    int i, j;
    int cx = size, cy = size;
    for (j = 0; j < height * size; ++j) {
        unsigned char* sx = src_p;
        unsigned char* px = dst_p;
        for (i = 0; i < width * size; ++i) {
            unsigned char a = *sx;
            if(--cx <= 0) {
            	cx = size;
            	sx ++;
            }
            if (a == 255) {
            	memcpy(px, &color, fb_bpp / 8);
            } else if (a > 0) {
            	int color2 = color | (a << 24);
                memcpy(px, &color2, fb_bpp / 8);
            }
			px += fb_bpp / 8;
        }
        if(--cy <= 0) {
        	cy = size;
        	src_p += src_row_bytes;
    	}
        dst_p += dst_row_bytes;
    }
}

int fb_font_width() {
	return font.cwidth;
}

int fb_font_height() {
	return font.cheight;
}

void fb_text(int x, int y, const char *s, int color, int bold, int size) {
    unsigned off;
    
    bold = bold && (font.height != font.cheight);

    while((off = *s++)) {
        off -= 32;
        if (outside(x, y) || outside(x + font.cwidth * size - 1, y + font.cheight - 1)) break;
        if (off < 96) {
            unsigned char* src_p = font_data + (off * font.cwidth) + (bold ? font.cheight * font.width : 0);
            unsigned char* dst_p = (unsigned char*) fb_newbuf + y * fb_xsize + x * fb_bpp / 8;

            text_blend(src_p, font.width, dst_p, fb_xsize, font.cwidth, font.cheight, color, size);
        }
        x += font.cwidth * size;
    }
}

#define FB_ASSERT_POINT(x,y) assert((x >= 0 && x < fb_width) && (y >= 0 && y < fb_height)) 
#define FB_ASSERT_RECT(x,y,w,h) assert((x >= 0 && y >= 0) && (w > 0 && h > 0) && (x + w <= fb_width && y + h <= fb_height))

void fb_fill_rect(int x, int y, int width, int height, unsigned int color) {
	char *p, *p2;

	FB_ASSERT;
	FB_ASSERT_RECT(x, y, width, height);
	
	p2 = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
	for(y = 0; y < height; y ++) {
		p = p2;
		for(x = 0; x < width; x ++) {
			memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

void fb_draw_rect(int x, int y, int width, int height, unsigned int color, int weight) {
	char *p, *p2;

	FB_ASSERT;
	FB_ASSERT_RECT(x, y, width, height);
	
	p2 = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
	for(y = 0; y < height; y ++) {
		p = p2;
		for(x = 0; x < width; x ++) {
			if(y < weight || y >= height - weight || x < weight || x >= width - weight) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

void fb_draw_line(int x1, int y1, int x2, int y2, unsigned int color, int weight) {
	int x, y;
	char *p, *p2;
	int cx, cy;
	int minX, maxX;
	int minY, maxY;
	double x0, y0, radius, cR, cY;

	FB_ASSERT_POINT(x1, y1);
	FB_ASSERT_POINT(x2, y2);
	
	x0 = (x1 + x2) / 2.0f;
	y0 = (y1 + y2) / 2.0f;
	
	cx = x1 - x2;
	cy = y1 - y2;
	
	cR = sqrt((double)(cy*cy+cx*cx));
	
	radius = sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2)) + weight;
	
	minX = min(x1, x2) - weight * 2;
	maxX = max(x1, x2) + weight * 2;
	minY = min(y1, y2) - weight * 2;
	maxY = max(y1, y2) + weight * 2;
	if(minX < 0) minX = 0;
	if(minY < 0) minY = 0;
	if(maxX >= fb_width) maxX = fb_width - 1;
	if(maxY >= fb_height) maxY = fb_height - 1;

	p2 = fb_newbuf + minY * fb_xsize + minX * fb_bpp / 8;
	for(y = minY; y <= maxY; y ++) {
		p = p2;
		cY = pow(y - y0, 2);
		for(x = minX; x <= maxX; x ++) {
			if(sqrt(pow(x - x0, 2) + cY) < radius && (cx || cy) && abs(x*cy-y*cx-x1*cy+y1*cx)/cR <= weight / 2.0f) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

void fb_fill_oval(int x, int y, int width, int height, unsigned int color) {
	char *p, *p2;
	double a, b;
	double fx1,fx2,fy1,fy2;
	double f;

	FB_ASSERT;
	FB_ASSERT_RECT(x, y, width, height);

	a = width / 2.0f;
	b = height / 2.0f;
	if(a > b) {
		f = sqrt(a * a - b * b);
		fx1 = a - f;
		fx2 = a + f;
		fy1 = fy2 = b;
		f = 2.0f * a;
	} else {
		f = sqrt(b * b - a * a);
		fx1 = fx2 = a;
		fy1 = b - f;
		fy2 = b + f;
		f = 2.0f * b;
	}
	
	p2 = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
	for(y = 0; y < height; y ++) {
		p = p2;
		for(x = 0; x < width; x ++) {
			a = sqrt(pow(x - fx1, 2) + pow(y - fy1, 2)) + sqrt(pow(x - fx2, 2) + pow(y - fy2, 2));
			if(a <= f) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

void fb_draw_oval(int x, int y, int width, int height, unsigned int color, int weight) {
	char *p, *p2;
	double a, b;
	double fx1,fx2,fy1,fy2;
	double f;

	FB_ASSERT;
	FB_ASSERT_RECT(x, y, width, height);

	a = width / 2.0f;
	b = height / 2.0f;
	if(a > b) {
		f = sqrt(a * a - b * b);
		fx1 = a - f;
		fx2 = a + f;
		fy1 = fy2 = b;
		f = 2.0f * a;
	} else {
		f = sqrt(b * b - a * a);
		fx1 = fx2 = a;
		fy1 = b - f;
		fy2 = b + f;
		f = 2.0f * b;
	}
	
	p2 = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
	for(y = 0; y < height; y ++) {
		p = p2;
		for(x = 0; x < width; x ++) {
			a = sqrt(pow(x - fx1, 2) + pow(y - fy1, 2)) + sqrt(pow(x - fx2, 2) + pow(y - fy2, 2)) - f;
			if(a <= 0 && a >= -weight*2.0f) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

void fb_fill_circle(int x, int y, int radius, unsigned int color) {
	char *p, *p2;
	int side = radius * 2;
	x -= radius;
	y -= radius;

	FB_ASSERT;
	FB_ASSERT_RECT(x, y, side, side);
	
	p2 = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
	for(y = 0; y < side; y ++) {
		p = p2;
		for(x = 0; x < side; x ++) {
			if(sqrt(pow(x - radius, 2) + pow(y - radius, 2)) <= radius) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

void fb_draw_circle(int x, int y, int radius, unsigned int color, int weight) {
	char *p, *p2;
	int side = radius * 2;
	int r;

	x -= radius;
	y -= radius;

	FB_ASSERT;
	FB_ASSERT_RECT(x, y, side, side);
	
	p2 = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
	for(y = 0; y < side; y ++) {
		p = p2;
		for(x = 0; x < side; x ++) {
			r = sqrt(pow(x - radius, 2) + pow(y - radius, 2));
			if(r < radius && r >= radius - weight) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

