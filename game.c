#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <termio.h>

#include "fb.h"

volatile unsigned int is_running = 1;

void game_key(int key);
void game_render(void);

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

int main(int argc, char *argv[]) {
	int ret;
	struct timeval tv;
	struct termios newset, oldset;
	
	if(argc >= 3) fb_debug = atoi(argv[2]);

	if(argc >= 2) ret = fb_init(argv[1]);
	else ret = fb_init("/dev/fb0");
	
	if(ret == FB_ERR) return 1;

	signal(SIGINT, signal_handler);

	if(fb_save() == FB_ERR) fprintf(stderr, "save failed\n");

	tv.tv_sec = 0;
	tv.tv_usec = 40000; // 40ms
	
	// Single character key for one-time reading
	tcgetattr(0, &oldset);
	memcpy(&newset, &oldset, sizeof(oldset));
	newset.c_lflag &= (~ICANON);
	newset.c_cc[VTIME] = 0;
	newset.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &newset);

	while(is_running) {
		fd_set set;
		
		FD_ZERO(&set);
		FD_SET(0, &set); // stdin
		
		ret = select(1, &set, NULL, NULL, &tv);
		if(ret > 0) {
			game_key(getchar());
		} else if(ret == 0) {
			tv.tv_sec = 0;
			tv.tv_usec = 40000; // 40ms
			game_render();
		} else {
			perror("select stdin error");
			break;
		}
	};

	// restore old set
	tcsetattr(0, TCSANOW, &oldset);

	if(fb_restore() == FB_ERR) fprintf(stderr, "restore failed\n");

	fb_free();
	return 0;
}

void game_key(int key) {
	printf("        \r");
	switch(key) {
		case 'q':
		case 'Q':
			is_running = 0;
			break;
		case '4':
		case 'a':
		case 'A':
		case 'j':
		case 'J': // left move
			printf("L       \r");
			break;
		case '6':
		case 'd':
		case 'D':
		case 'l':
		case 'L': // right move
			printf("R       \r");
			break;
		case '5':
		case 's':
		case 'S':
		case 'k':
		case 'K': // down move
			printf("D       \r");
			break;
		case '8':
		case 'w':
		case 'W':
		case 'i':
		case 'I': // transshape
			printf("T       \r");
			break;
		case ' ':
		case '0':
			printf("_       \r");
			break;
		default:
			printf("        \r");
			break;
	}
}

#define FB_W (fb_width - 1)
#define FB_H (fb_height - 1)

void game_render(void) {
	// fb_fill_rect(0, 0, fb_width, fb_height, 0xffffffff); // background

	fb_fill_rect(0, 0, 100, 100, 0xffff0000); // left top
	fb_fill_rect(0, FB_H - 100, 100, 100, 0xffff0000); // left bottom
	fb_fill_rect(FB_W - 100, 0, 100, 100, 0xffff0000); // right top
	fb_fill_rect(FB_W - 100, FB_H - 100, 100, 100, 0xffff0000); // right bottom
	fb_fill_rect(100, 100, FB_W - 200, FB_H - 200, 0xffff0000); // center
}

