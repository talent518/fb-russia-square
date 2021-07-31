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
#include <time.h>

#include <termio.h>

#include "fb.h"

volatile unsigned int is_running = 1;

void game_key(int key);
void game_init(void);
void game_render(void);
void game_timer(void);
void game_alrm(void);

static void signal_handler(int sig) {
	switch(sig) {
		case SIGPIPE:
		case SIGTERM:
		case SIGINT:
			is_running = 0;
			break;
		case SIGALRM:
			if(is_running) game_alrm();
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
	
	signal(SIGALRM, signal_handler);
	game_timer();

	game_init();

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

void game_timer(void) {
	struct itimerval itv;

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 40000; // 40ms

	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 5000; // 5ms

	setitimer(ITIMER_REAL, &itv, NULL);
}

void game_key_trans(void);
void game_key_left(void);
void game_key_right(void);
void game_key_down(void);
void game_key_fall(void);

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
			game_key_left();
			break;
		case '6':
		case 'd':
		case 'D':
		case 'l':
		case 'L': // right move
			printf("R       \r");
			game_key_right();
			break;
		case '5':
		case 's':
		case 'S':
		case 'k':
		case 'K': // down move
			printf("D       \r");
			game_key_down();
			break;
		case '8':
		case 'w':
		case 'W':
		case 'i':
		case 'I': // transshape
			printf("T       \r");
			game_key_trans();
			break;
		case ' ':
		case '0':
			printf("_       \r");
			game_key_fall();
			break;
		default:
			printf("        \r");
			break;
	}
}

#define WS_PX 15
#define HEIGHT_SHAPE_NUM 20
#define WIDTH_SHAPE_NUM 10
#define MAX_GRADE 25
static const int SHAPES[] = {0x4444, 0x4460, 0x2260, 0x0C60, 0x06C0, 0x0660, 0x04E0};
static const int SHAPE_NUM = sizeof(SHAPES) / sizeof(int);
static const int COLORS[] = {0x33, 0x66, 0x99, 0xcc};
static const int COLOR_NUM = sizeof(COLORS) / sizeof(int);
const char *HELPS[] = {
	"       HELP     ",
	"----------------",
	"Trans: 8 w W i I",
	" Left: 4 a A j J",
	"Right: 6 d D l L",
	" Down: 5 s S k K",
	" Fall: 0 Space"
};
const int HELPLEN = sizeof(HELPS) / sizeof(HELPS[0]);

static int sX, sY, scoreNum, lineNum;
static bool squareRecords[HEIGHT_SHAPE_NUM][WIDTH_SHAPE_NUM];
static int colorRecords[HEIGHT_SHAPE_NUM][WIDTH_SHAPE_NUM];
static int curShape, nextShape;
static int curColor, nextColor;
static int overColor;
static bool endGame;
static int maxGrade = MAX_GRADE;
static int idxGrade = 0;

void game_alrm(void) {
	if(++idxGrade >= maxGrade) {
		idxGrade = 0;
		PROF(game_render);
	}
	fb_sync();
}

int game_rand_color() {
	int r, g, b;
    r = rand() % COLOR_NUM;
    g = rand() % COLOR_NUM;
    b = rand() % COLOR_NUM;
    return (COLORS[r] << 16) | (COLORS[g] << 8) | COLORS[b] | 0xff000000;
}

void game_reset(void) {
	int x, y;

	srand(time(NULL));
	
	endGame = false;
    sX = sY = 0;
    curShape = 0;
    curColor = 0;
    nextShape = SHAPES[rand() % SHAPE_NUM];
    nextColor = game_rand_color();
    scoreNum = lineNum = 0;
    for(y = 0; y < HEIGHT_SHAPE_NUM; y ++) {
        for(x = 0; x < WIDTH_SHAPE_NUM; x ++) {
            squareRecords[y][x] = false;
            colorRecords[y][x] = 0;
        }
    }
}

void game_init(void) {
	game_reset();

	game_render();
	fb_sync();
}

void game_draw(int x, int y, int side, int color) {
	fb_draw_line(x, y, x + side - 1, y, color + 0x333333, 1);
	fb_draw_line(x, y, x, y + side - 1, color + 0x333333, 1);
	fb_draw_line(x + side - 1, y, x + side - 1, y + side - 1, color - 0x333333, 1);
	fb_draw_line(x, y + side - 1, x + side - 1, y + side - 1, color - 0x333333, 1);
	fb_fill_rect(x + 1, y + 1, side - 2, side - 2, color);
}

bool game_shape_point(int p, int x, int y) {
    return (p & (1 << (16 - (x + 1 + y * 4))));
}

static bool is_help = true;
void game_render(void) {
	const int side = fb_height / (HEIGHT_SHAPE_NUM + 2);
	const int X = (fb_width - (WIDTH_SHAPE_NUM + 5) * side) / 2, Y = (fb_height - HEIGHT_SHAPE_NUM * side) / 2;
	int x, y;
	int x2, y2;

	// draw main box
	for(y = 0; y < HEIGHT_SHAPE_NUM; y ++) {
		for(x = 0; x < WIDTH_SHAPE_NUM; x ++) {
			x2 = X + x * side;
			y2 = Y + y * side;
			if(squareRecords[y][x]) {
				game_draw(x2, y2, side, colorRecords[y][x]);
			} else {
				fb_fill_rect(x2, y2, side, side, 0xffffffff);
			}
		}
	}

	// draw next box
	for(y = 0; y < 4; y ++) {
		for(x = 0; x < 4; x ++) {
			x2 = X + (WIDTH_SHAPE_NUM + 1) * side + x * side;
			y2 = Y + y * side;
			if(game_shape_point(nextShape, x, y)) {
				game_draw(x2, y2, side, nextColor);
			} else {
				fb_fill_rect(x2, y2, side, side, 0xffffffff);
			}
		}
	}
	
	// draw scores and lines
	{
		int x, y;
		char str[20];
		x = X + (WIDTH_SHAPE_NUM + 1) * side;
		y = Y + 4.5f * side;

		sprintf(str, "%d", scoreNum);
		fb_fill_rect(x, y, fb_font_width() * (strlen(str) + 7), fb_font_height(), 0);
		fb_text(x, y, "SCORE:", 0xffffffff, 1);
		fb_text(x + fb_font_width() * 7, y, str, 0xffff0000, 1);

		y += fb_font_height() * 1.2f;

		sprintf(str, "%d", scoreNum);
		fb_fill_rect(x, y, fb_font_width() * (strlen(str) + 7), fb_font_height(), 0);
		fb_text(x, y, " LINE:", 0xffffffff, 1);
		fb_text(x + fb_font_width() * 7, y, str, 0xffff3300, 1);
	}
	
	// draw help
	if(is_help) {
		int x, y, i;
		int fh = fb_font_height() * 1.2f;

		is_help = false;

		x = X + (WIDTH_SHAPE_NUM + 1) * side;
		y = Y + (HEIGHT_SHAPE_NUM * side - HELPLEN * fh) / 2;

		for(i = 0; i < HELPLEN; i ++) fb_text(x, y + i * fh, HELPS[i], 0xff666666, 0);
	}
	
	// draw lines
	{
		int x, y;
		char str[10];
		x = X + (WIDTH_SHAPE_NUM + 1) * side + (4 * side - fb_font_width() * 8) / 2;
		y = Y + (HEIGHT_SHAPE_NUM - 4) * side - fb_font_height() - 5;
		fb_fill_rect(x, y, fb_font_width() * 8, fb_font_height(), 0);
		fb_text(x, y, str, 0xffffffff, 1);
	}
	
	// draw time
	{
		const time_t t = time(NULL);
		struct tm tm;
		const int radius = side * 2;
		const int x0 = X + (WIDTH_SHAPE_NUM + 3) * side, y0 = Y + (HEIGHT_SHAPE_NUM - 2) * side;
		double angle;
		int weight;

		localtime_r(&t, &tm);
		
		{
			int x, y;
			char str[10];
			sprintf(str, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
			x = X + (WIDTH_SHAPE_NUM + 1) * side + (4 * side - fb_font_width() * 8) / 2;
			y = Y + (HEIGHT_SHAPE_NUM - 4) * side - fb_font_height() - 5;
			fb_fill_rect(x, y, fb_font_width() * 8, fb_font_height(), 0);
			fb_text(x, y, str, 0xffffffff, 1);
		}
		
		// background and border
		fb_fill_circle(x0, y0, radius - 1, 0);
		fb_draw_circle(x0, y0, radius, 0xffffffff, 1);

		// scale
		{
			int i;
			for(i = 0; i < 12; i ++) {
				weight = (i % 3 == 0 ? 5 : 1);
				angle = i * 30.0f * M_PI / 180.0f;
				x = (radius - weight) * cos(angle) + x0;
				y = -(radius - weight) * sin(angle) + y0;
				x2 = radius * 0.85f * cos(angle) + x0;
				y2 = -radius * 0.85f * sin(angle) + y0;
				fb_draw_line(x2, y2, x, y, 0xffffffff, weight);
			}
		}

		// hour
		{
			angle = (90.0f - tm.tm_hour * 30.0f - tm.tm_min * 0.5f - tm.tm_sec / 120.0f) * M_PI / 180.0f;
			x = radius * 0.35f * cos(angle) + x0;
			y = -radius * 0.35f * sin(angle) + y0;
			fb_draw_line(x0, y0, x, y, 0xff0000ff, 5);
		}

		// minute
		{
			angle = (90.0f - tm.tm_min * 6.0f - tm.tm_sec / 10.0f) * M_PI / 180.0f;
			x = radius * 0.6f * cos(angle) + x0;
			y = -radius * 0.55f * sin(angle) + y0;
			fb_draw_line(x0, y0, x, y, 0xff00ff00, 3);
		}

		// second
		{
			angle = (90.0f - tm.tm_sec * 6) * M_PI / 180.0f;
			x = radius * 0.75f * cos(angle) + x0;
			y = -radius * 0.75f * sin(angle) + y0;
			fb_draw_line(x0, y0, x, y, 0xffff0000, 1);
		}
	}
}

// transshape
void game_key_trans(void) {
}

// left move
void game_key_left(void) {
}

// right move
void game_key_right(void) {
}

// down move
void game_key_down(void) {
}

// fall
void game_key_fall(void) {
}


