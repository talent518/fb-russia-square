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

#define FB_ASSERT assert(fb_fd > 0 && fb_addr)

int fb_debug = 0, fb_width = 0, fb_height = 0, fb_bpp = 0;

static int fb_fd = 0;
static char *fb_addr = NULL;
static int fb_size = 0;
static int fb_xoffset = 0, fb_xsize = 0;
static char *fb_buf = NULL;

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
	
	munmap(fb_addr, fb_size);
	fb_addr = NULL;
	fb_size = 0;

	close(fb_fd);
	fb_fd = 0;

	return FB_OK;
}

int fb_save(void) {
	register char *p, *p2;
	register int i;

	FB_ASSERT;

	if(fb_buf) free(fb_buf);

	fb_buf = (char*) malloc(fb_xsize * fb_height);
	if(fb_buf == NULL) return FB_ERR;

	p = fb_buf;
	p2 = fb_addr;
	for(i = 0; i < fb_height; i ++) {
		memcpy(p, p2, fb_xsize);
		memset(p2, 0, fb_xsize);
		
		p += fb_xsize;
		p2 += fb_xoffset;
	}
	
	return FB_OK;
}

int fb_restore(void) {
	register char *p, *p2;
	register int i;

	FB_ASSERT;
	if(fb_buf == NULL) return FB_ERR;

	p = fb_buf;
	p2 = fb_addr;
	for(i = 0; i < fb_height; i ++) {
		memcpy(p2, p, fb_xsize);
		
		p += fb_xsize;
		p2 += fb_xoffset;
	}
	
	free(fb_buf);
	fb_buf = NULL;

	return FB_OK;
}

#define FB_ASSERT_RECT assert((x >= 0 && x < fb_width) && (y >= 0 && y < fb_height) && (width > 0 && x + width <= fb_width) && (height > 0 && y + height <= fb_height))

int fb_fill_rect(int x, int y, int width, int height, unsigned int color) {
	char *p, *p2;

	FB_ASSERT;
	FB_ASSERT_RECT;
	
	p2 = fb_addr + y * fb_xoffset + x * fb_bpp / 8;
	for(y = 0; y < height; y ++) {
		p = p2;
		for(x = 0; x < width; x ++) {
			memcpy(p, &color, fb_bpp / 8);
			p += fb_bpp / 8;
		}
		p2 += fb_xoffset;
	}
	
	return FB_OK;
}

