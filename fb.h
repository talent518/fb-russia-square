#ifndef _FB_H
#	define _FB_H

#define FB_OK 0
#define FB_ERR -1

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

typedef unsigned int uint;

extern int fb_width;
extern int fb_height;
extern int fb_bpp;
extern int fb_debug;

int fb_init(const char *path);
int fb_free(void);

int fb_save(void);
int fb_restore(void);

void fb_sync(void);

void fb_fill_rect(int x, int y, int width, int height, unsigned int color);
void fb_fill_round_rect(int x, int y, int width, int height, unsigned int color, int corner);
void fb_fill_oval(int x, int y, int width, int height, unsigned int color);
void fb_fill_circle(int x, int y, int radius, unsigned int color);

void fb_draw_line(int x1, int y1, int x2, int y2, unsigned int color, int weight);

void fb_draw_rect(int x, int y, int width, int height, unsigned int color, int weight);
void fb_draw_round_rect(int x, int y, int width, int height, unsigned int color, int weight, int corner);
void fb_draw_oval(int x, int y, int width, int height, unsigned int color, int weight);
void fb_draw_circle(int x, int y, int radius, unsigned int color, int weight);

#endif

