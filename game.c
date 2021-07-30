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
		case SIGALRM:
			if(is_running) fb_sync();
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

	signal(SIGPIPE, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if(fb_save() == FB_ERR) fprintf(stderr, "save failed\n");
	
	// Single character key for one-time reading
	tcgetattr(0, &oldset);
	memcpy(&newset, &oldset, sizeof(oldset));
	newset.c_lflag &= (~ICANON);
	newset.c_cc[VTIME] = 0;
	newset.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &newset);
	
	{
		struct itimerval itv;

		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 30000; // 30ms

		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = 1000; // 30ms

		signal(SIGALRM, signal_handler);
		setitimer(ITIMER_REAL, &itv, NULL);
	}

	game_render();

	fprintf(stdout, "\033[?25l"); // hide cursor
	fflush(stdout);
	while(is_running) {
		fd_set set;
		
		FD_ZERO(&set);
		FD_SET(0, &set); // stdin

		tv.tv_sec = 0;
		tv.tv_usec = 40000; // 40ms
		
		ret = select(1, &set, NULL, NULL, &tv);
		if(ret > 0) {
			game_key(getchar());
		} else if(ret == 0) {
			game_render();
		} else if(errno != EINTR) {
			perror("select stdin error");
			is_running = 0;
		}
	};
	fprintf(stdout, "\033[?25h"); // show cursor
	fflush(stdout);

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

static bool is_bg = true;

void game_render(void) {
	if(is_bg) {
		
		
		is_bg = false;
	}
}

