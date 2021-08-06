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

int fb_init(const char *path);
int fb_free(void);

int fb_save(void);
int fb_restore(void);

void fb_sync(void);

int fb_color(int red, int green, int blue);
int fb_color_add(int color, int add);

typedef enum {
	FONT_10x18 = 1,
	FONT_12x22,
	FONT_18x32,
} font_family_t;

void fb_set_font(font_family_t family);

int fb_font_width();
int fb_font_height();

void fb_text(int x, int y, const char *s, int color, int bold, int size);

void fb_fill_rect(int x, int y, int width, int height, unsigned int color);
void fb_fill_round_rect(int x, int y, int width, int height, unsigned int color, int corner);
void fb_fill_oval(int x, int y, int width, int height, unsigned int color);
void fb_fill_circle(int x, int y, int radius, unsigned int color);

void fb_draw_line(int x1, int y1, int x2, int y2, unsigned int color, int weight);

void fb_draw_rect(int x, int y, int width, int height, unsigned int color, int weight);
void fb_draw_round_rect(int x, int y, int width, int height, unsigned int color, int weight, int corner);
void fb_draw_oval(int x, int y, int width, int height, unsigned int color, int weight);
void fb_draw_circle(int x, int y, int radius, unsigned int color, int weight);

void fb_draw_point(int x, int y, unsigned int color);

static inline double microtime() {
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double) tp.tv_sec + (double) tp.tv_usec / 1000000.0f;
}

#ifndef DTIME
#	define PROF(f,args...) f(args)
#	define BEGIN_TIME()
#	define END_TIME()
#else
#	define PROF(f,args...) do { \
		double __t = microtime(); \
		f(args); \
		printf(#f ": %lf\n", microtime() - __t); \
	} while(0)
#	define BEGIN_TIME() do { double __t = microtime()
#	define END_TIME() printf("[TIME] %s:%d %lf\n", __func__, __LINE__, microtime() - __t); } while(0)
#endif

#ifdef DEBUG
#	define dprintf(fmt, args...) do {printf("[INFO] " fmt, ##args);fflush(stdout);} while(0)
#	define eprintf(fmt, args...) do {fprintf(stderr, "[WARN] " fmt, ##args);fflush(stderr);} while(0)
#	define pprintf(fmt, args...) do {fprintf(stderr, "[ ERR] " fmt ": %s\n", ##args, strerror(errno));fflush(stderr);} while(0)
#else
#	define dprintf(fmt, args...) while(0)
#	define eprintf(fmt, args...) while(0)
#	define pprintf(fmt, args...) while(0)
#endif

#endif

