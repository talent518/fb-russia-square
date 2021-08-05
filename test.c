#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <signal.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <math.h>

#include "fb.h"

volatile unsigned int is_running = 1;

static void signal_handler(int sig) {
	switch(sig) {
		case SIGPIPE:
		case SIGTERM:
		case SIGINT:
			is_running = 0;
			break;
		default:
			printf("SIG: %d\n", sig);
	}
}

#define FB_W fb_width
#define FB_H fb_height

int main(int argc, char *argv[]) {
	int ret;

	if(argc >= 2) ret = fb_init(argv[1]);
	else ret = fb_init("/dev/fb0");
	
	if(ret == FB_ERR) return 1;

	signal(SIGPIPE, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if(fb_save() == FB_ERR) fprintf(stderr, "save failed\n");

	{
		BEGIN_TIME();

		fb_fill_rect(0, 0, fb_width, fb_height, 0xffffffff); // background

		fb_fill_rect(0, 0, 100, 100, 0xffff0000); // left top
		fb_fill_rect(0, FB_H - 100, 100, 100, 0xffff0000); // left bottom
		fb_fill_rect(FB_W - 100, 0, 100, 100, 0xffff0000); // right top
		fb_fill_rect(FB_W - 100, FB_H - 100, 100, 100, 0xffff0000); // right bottom
		fb_fill_rect(100, 100, FB_W - 200, FB_H - 200, 0xffff0000); // center
		fb_draw_rect(150, 150, FB_W - 300, FB_H - 300, 0xffffff00, 2); // center
		fb_draw_rect((FB_W - 300) / 2, FB_H - 100, 300, 100, 0xff0000ff, 2); // center

		fb_fill_circle(FB_W / 2, FB_H / 2, 100, 0xffffffff);
		fb_draw_circle(FB_W / 2, FB_H / 2, 120, 0xffffffff, 2);

		fb_draw_oval((FB_W - 300) / 2, (FB_H - 400) / 2, 300, 400, 0xffffffff, 2); // draw oval in center
		fb_draw_oval((FB_W - 400) / 2, (FB_H - 300) / 2, 400, 300, 0xffffffff, 2); // draw oval in center

		fb_draw_oval(120, 120, FB_W - 240, FB_H - 240, 0xff00ffff, 1); // draw oval in center
		fb_fill_oval((FB_W - 200) / 2, 0, 200, 100, 0xff00ffff); // fill oval in top center
		fb_fill_oval(0, (FB_H - 200) / 2, 100, 200, 0xff00ffff); // fill oval in left center
		fb_fill_oval((FB_W - 100) / 2, (FB_H - 100) / 2, 100, 100, 0xff00ffff); // fill oval in center
		fb_draw_oval((FB_W - 150) / 2, (FB_H - 150) / 2, 150, 150, 0xff00ffff, 2); // draw oval in center

		END_TIME();
		BEGIN_TIME();

		fb_draw_line(0, 0, 100, 100, 0xff0000ff, 2);
		fb_draw_line(0, 0, 100, 200, 0xff0000ff, 2);
		fb_draw_line(0, 0, 100, 300, 0xff0000ff, 2);
		fb_draw_line(0, 0, 200, 100, 0xff0000ff, 2);
		fb_draw_line(0, 0, 300, 100, 0xff0000ff, 2);

		END_TIME();
		BEGIN_TIME();

		fb_draw_line(0, FB_H / 2, FB_W - 1, FB_H / 2, 0xff0000ff, 2);
		fb_draw_line(FB_W / 2, 0, FB_W / 2, FB_H - 1, 0xff0000ff, 2);

		fb_draw_line(0, FB_H / 2 + 2, FB_W - 1, FB_H / 2 + 2, 0xff00ff00, 2);
		fb_draw_line(FB_W / 2 + 2, 0, FB_W / 2 + 2, FB_H - 1, 0xff00ff00, 2);
		
		END_TIME();
		BEGIN_TIME();
		
		{
			int x = (FB_W / 2), y = (FB_H / 2);
			int r = min(FB_W / 3, FB_H / 3);
			int x1 = x, y1 = y - r;
			int x2, y2;
			int i;
			double rad;

			for(i = 144; i <= 720; i += 144 ) {
				rad = (90 + i) * M_PI / 180.0;
				x2 = r * cos(rad) + x;
				y2 = - r * sin(rad) + y;
				fb_draw_line(x1, y1, x2, y2, 0xff00ff00, 1);
				x1 = x2;
				y1 = y2;
			}
		}

		END_TIME();
		BEGIN_TIME();

		fb_text(100, 75, "Hello World!", 0xff000000, 0, 1);
		fb_text(100, 100, "Hello World!", 0xff000000, 1, 2);

		END_TIME();
	}

	BEGIN_TIME();
	{ // Archimedean spiral
		int x, y;
		int x0 = FB_W / 2, y0 = FB_H / 2;
		int color = fb_color(0xff, 0, 0xff);
		int i = 0;
		double r = 1.0f, rad;
		double angle = 1.0f;
		double R = sqrt(pow(x0, 2) + pow(y0, 2));
		do {
			i++;
			// dprintf("%d: %lf\n", i, angle);
			rad = angle * M_PI / 180.0f;
			r = 5.0f * rad;
			x = x0 + r * cos(rad);
			y = y0 - r * sin(rad);
			if(x >= 0 && x < fb_width && y >= 0 && y < fb_height) fb_draw_point(round(x), round(y), color);
			rad = 360.0f / (2 * M_PI * r);
			if(rad > 10.0f) angle += 10.0f;
			else angle += rad;
		} while(r < R);
		dprintf("points: %d\n", i);
	}
	END_TIME();

	fprintf(stdout, "\033[?25l"); // hide cursor
	fflush(stdout);
	while(is_running) {
		fb_sync();
		usleep(40000);
	}
	fprintf(stdout, "\033[?25h"); // show cursor
	fflush(stdout);

	if(fb_restore() == FB_ERR) fprintf(stderr, "restore failed\n");

	fb_free();
	return 0;
}

