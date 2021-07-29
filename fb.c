#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

int fb_debug = 0, fb_width = 0, fb_height = 0, fb_bpp = 0;

static int fb_fd = 0;
static char *fb_addr = NULL;
static int fb_size = 0;
static int fb_xoffset = 0, fb_xsize = 0;
static char *fb_oldbuf = NULL, *fb_newbuf = NULL;

int fb_init(const char *path) {
	struct fb_var_screeninfo vinfo;

	assert(fb_fd == 0 && fb_addr == NULL);
	
	fb_fd = open(path, O_RDWR);
	if(fb_fd < 0) {
		perror("open framebuffer device failed");
		return FB_ERR;
	}

	if(ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("ioctl FBIOGET_VSCREENINFO failed");
		close(fb_fd);
		return FB_ERR;
	}

	fb_width = vinfo.xres;
	fb_xoffset = vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
	fb_xsize = vinfo.xres * vinfo.bits_per_pixel / 8;
	fb_height = vinfo.yres;
	fb_bpp = vinfo.bits_per_pixel;

	fb_newbuf = (char*) malloc(fb_xsize * fb_height);
	if(fb_newbuf == NULL) {
		perror("malloc fb_newbuf failed");
		close(fb_fd);
		return FB_ERR;
	}
	memset(fb_newbuf, 0, fb_xsize * fb_height);

	fb_size = vinfo.xres_virtual * vinfo.yres_virtual * fb_bpp / 8;
	fb_addr = (char*) mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if(fb_addr == MAP_FAILED) {
		perror("mmap failed");
		close(fb_fd);
		return FB_ERR;
	}

	if(fb_debug) fprintf(stderr, "size: %dx%d, bpp: %d, mmap: %p\n", fb_width, fb_height, fb_bpp, fb_addr);

	return FB_OK;
}

int fb_free(void) {
	FB_ASSERT;
	
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
	register char *p, *p2;
	register int i;
	
	p = fb_newbuf;
	p2 = fb_addr;
	for(i = 0; i < fb_height; i ++) {
		memcpy(p2, p, fb_xsize);
		
		p += fb_xsize;
		p2 += fb_xoffset;
	}
}

int fb_save(void) {
	register char *p, *p2;
	register int i;

	FB_ASSERT;

	if(fb_oldbuf) free(fb_oldbuf);

	fb_oldbuf = (char*) malloc(fb_xsize * fb_height);
	if(fb_oldbuf == NULL) return FB_ERR;

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
	register char *p, *p2;
	register int i;

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

void fb_draw_line(int x1, int y1, int x2, int y2, unsigned int color, int weight) {
	register int x, y;
	register char *p;
	int minX, minY;
	int maxX, maxY;

	FB_ASSERT_POINT(x1, y1);
	FB_ASSERT_POINT(x2, y2);

	if(x1 == x2) {
		minY = min(y1, y2);
		maxY = max(y1, y2);
		
		p = fb_newbuf + minY * fb_xsize + x1 * fb_bpp / 8;
		for(y = minY; y <= maxY; y++) {
			memcpy(p, &color, fb_bpp / 8);
			p += fb_xsize;
		}
	} else {
		minX = min(x1, x2);
		maxX = max(x1, x2);

		if(y1 == y2) {
			p = fb_newbuf + y1 * fb_xsize + minX * fb_bpp / 8;
			for(x = minX; x <= maxX; x ++) {
				memcpy(p, &color, fb_bpp / 8);
				p += fb_bpp / 8;
			}
		} else {
			double k = (double) (y2-y1) / (double) (x2-x1);
			for(x = minX; x <= maxX; x ++) {
				y = k * (x - x1) + y1;
				p = fb_newbuf + y * fb_xsize + x * fb_bpp / 8;
				memcpy(p, &color, fb_bpp / 8);
			}
		}
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
			if(a < 0) a = -a;
			if(a < weight) memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xsize;
	}
}

