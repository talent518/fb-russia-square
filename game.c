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
			dprintf("SIG: %d\n", sig);
	}
}

int main(int argc, char *argv[]) {
	int ret;
	struct timeval tv;
	struct termios newset, oldset;

	if(argc >= 2) ret = fb_init(argv[1]);
	else ret = fb_init("/dev/fb0");
	
	if(ret == FB_ERR) return 1;

	signal(SIGPIPE, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if(fb_save() == FB_ERR) eprintf("save failed\n");
	
	// Single character key for one-time reading
	tcgetattr(0, &oldset);
	memcpy(&newset, &oldset, sizeof(oldset));
	newset.c_lflag &= (~ICANON);
	newset.c_cc[VTIME] = 0;
	newset.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &newset);

	if(write(0, " ", 1) <= 0) pprintf("write");
	
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
		} else if(ret != 0 && errno != EINTR) {
			pprintf("select stdin error");
			is_running = 0;
		}
	};
	fprintf(stdout, "\033[?25h"); // show cursor
	fflush(stdout);

	// restore old set
	tcsetattr(0, TCSANOW, &oldset);

	if(fb_restore() == FB_ERR) eprintf("restore failed\n");

	fb_free();
	return 0;
}

void game_timer(void) {
	struct itimerval itv;

	itv.it_interval.tv_sec = itv.it_value.tv_sec = 0;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = 40000; // 40ms

	setitimer(ITIMER_REAL, &itv, NULL);
}

void game_key_trans(void);
void game_key_left(void);
void game_key_right(void);
void game_key_down(void);
void game_key_fall(void);
void game_start(void);
void game_pause(void);

void game_key(int key) {
	dprintf("        \r");
	switch(key) {
		case 'q':
		case 'Q':
			is_running = 0;
			break;
		case '[': // start game
			dprintf("S       \r");
			game_start();
			break;
		case ']': // pause game
			dprintf("P       \r");
			game_pause();
			break;
		case '4':
		case 'a':
		case 'A':
		case 'j':
		case 'J': // left move
			dprintf("L       \r");
			game_key_left();
			break;
		case '6':
		case 'd':
		case 'D':
		case 'l':
		case 'L': // right move
			dprintf("R       \r");
			game_key_right();
			break;
		case '5':
		case 's':
		case 'S':
		case 'k':
		case 'K': // down move
			dprintf("D       \r");
			game_key_down();
			break;
		case '8':
		case 'w':
		case 'W':
		case 'i':
		case 'I': // transshape
			dprintf("T       \r");
			game_key_trans();
			break;
		case ' ':
		case '0':
			dprintf("_       \r");
			game_key_fall();
			break;
		default:
			dprintf("        \r");
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
	"Start: [",
	"Pause: ]",
	"Trans: 8 w W i I",
	" Left: 4 a A j J",
	"Right: 6 d D l L",
	" Down: 5 s S k K",
	" Fall: 0 Space",
	" Quit: q Q"
};
const int HELPLEN = sizeof(HELPS) / sizeof(HELPS[0]);

static int sX, sY, scoreNum, lineNum;
static bool squareRecords[HEIGHT_SHAPE_NUM][WIDTH_SHAPE_NUM];
static int colorRecords[HEIGHT_SHAPE_NUM][WIDTH_SHAPE_NUM];
static int curShape, nextShape;
static int curColor, nextColor;
static bool beginGame, endGame, pauseGame;
static int maxGrade = MAX_GRADE;
static int idxGrade = 0;

void game_alrm(void) {
	if(++idxGrade >= maxGrade) {
		idxGrade = 0;
		if(pauseGame || endGame) PROF(game_render);
		else PROF(game_key_down);
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
	
	beginGame = endGame = pauseGame = false;
	maxGrade = MAX_GRADE;
	idxGrade = 0;
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

void game_next_shape(void);
void game_start(void) {
	game_reset();
	beginGame = true;
	game_next_shape();
	game_render();
}

void game_pause(void) {
	pauseGame = !pauseGame;
	idxGrade = 0;
	game_render();
	fb_sync();

	game_timer();
}

void game_draw(int x, int y, int side, int color) {
#if 0
	fb_draw_line(x, y, x + side - 1, y, color + 0x333333, 1);
	fb_draw_line(x, y, x, y + side - 1, color + 0x333333, 1);
	fb_draw_line(x + side - 1, y, x + side - 1, y + side - 1, color - 0x333333, 1);
	fb_draw_line(x, y + side - 1, x + side - 1, y + side - 1, color - 0x333333, 1);
#else
	fb_fill_rect(x, y, 1, side, color + 0x333333);
	fb_fill_rect(x, y, side, 1, color + 0x333333);
	fb_fill_rect(x + side - 1, y, 1, side, color - 0x333333);
	fb_fill_rect(x, y + side - 1, side, 1, color - 0x333333);
#endif
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

	for(y = 0; y < HEIGHT_SHAPE_NUM; y ++) {
		for(x = 0; x < WIDTH_SHAPE_NUM; x ++) {
			x2 = X + x * side;
			y2 = Y + y * side;
			if(squareRecords[y][x]) { // draw main box
				game_draw(x2, y2, side, colorRecords[y][x]);
			} else if(x >= sX && x <= sX + 3 && y >= sY && y <= sY + 3 && game_shape_point(curShape, x - sX, y - sY)) { // draw current shape
				game_draw(x2, y2, side, curColor);
			} else { // draw main box
				fb_fill_rect(x2, y2, side, side, 0xffffffff);
			}
		}
	}

	for(y = 0; y < 4; y ++) {
		for(x = 0; x < 4; x ++) {
			// draw next shape
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
		char str[20];
		x = X + (WIDTH_SHAPE_NUM + 1) * side;
		y = Y + 4.5f * side;

		sprintf(str, "%d", scoreNum);
		fb_fill_rect(x, y, fb_font_width() * (strlen(str) + 7), fb_font_height(), 0);
		fb_text(x, y, "SCORE:", 0xffffffff, 1, 1);
		fb_text(x + fb_font_width() * 7, y, str, 0xffff0000, 1, 1);

		y += fb_font_height() * 1.2f;

		sprintf(str, "%d", lineNum);
		fb_fill_rect(x, y, fb_font_width() * (strlen(str) + 7), fb_font_height(), 0);
		fb_text(x, y, " LINE:", 0xffffffff, 1, 1);
		fb_text(x + fb_font_width() * 7, y, str, 0xffff3300, 1, 1);
	}
	
	// draw help
	if(is_help) {
		int i;
		int fh = fb_font_height() * 1.2f;

		is_help = false;

		x = X + (WIDTH_SHAPE_NUM + 1) * side;
		y = Y + (HEIGHT_SHAPE_NUM * side - HELPLEN * fh) / 2;

		for(i = 0; i < HELPLEN; i ++) fb_text(x, y + i * fh, HELPS[i], 0xff666666, 0, 1);
	}
	
	// draw lines
	{
		char str[10];
		x = X + (WIDTH_SHAPE_NUM + 1) * side + (4 * side - fb_font_width() * 8) / 2;
		y = Y + (HEIGHT_SHAPE_NUM - 4) * side - fb_font_height() - 5;
		fb_fill_rect(x, y, fb_font_width() * 8, fb_font_height(), 0);
		fb_text(x, y, str, 0xffffffff, 1, 1);
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
			char str[10];
			sprintf(str, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
			x = X + (WIDTH_SHAPE_NUM + 1) * side + (4 * side - fb_font_width() * 8) / 2;
			y = Y + (HEIGHT_SHAPE_NUM - 4) * side - fb_font_height() - 5;
			fb_fill_rect(x, y, fb_font_width() * 8, fb_font_height(), 0);
			fb_text(x, y, str, 0xffffffff, 1, 1);
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
	
	if(beginGame && (endGame || pauseGame)) {
		const char *str = endGame ? "GAME OVER" : "GAME PAUSE";
		const int sz = 3;
		const int fw = fb_font_width() * sz, fh = fb_font_height() * sz;
		const int color = game_rand_color();
		x = X + (WIDTH_SHAPE_NUM * side - fw * strlen(str)) / 2;
		y = Y + (HEIGHT_SHAPE_NUM * side - fh) / 2;
		fb_text(x - 1, y - 1, str, color + 0x333333, 1, sz);
		fb_text(x + 1, y + 1, str, color - 0x333333, 1, sz);
		fb_text(x, y, str, color, 1, sz);
	}
}

int game_rotate_shape(int shape){
	int s = 0, x, y;
	for(y = 0; y < 4; y ++)
		for(x = 0; x < 4; x ++)
			if(game_shape_point(shape, 4 - y - 1, x))
				s |= (1 << (16 - (x + 1 + y * 4)));
	return s;
}

bool game_movable_shape(int shape, int X, int Y) {
	int x, y;
	for(y = 0; y < 4; y ++)
		for(x = 0; x < 4; x ++)
			if(game_shape_point(shape, x, y) && (X + x < 0 || X + x >= WIDTH_SHAPE_NUM || Y + y >= HEIGHT_SHAPE_NUM || (Y + y >= 0 && squareRecords[Y + y][X + x])))
				return false;
	return true;
}

void game_save_shape(void) {
	int x, y, size = 0;
	bool flag;
	int idx = -1;
	int stack[4];

	for(y = 0; y < 4; y ++) {
		if(sY + y < 0) continue;

		for(x = 0; x < 4; x ++){
			if(game_shape_point(curShape, x, y)){
				squareRecords[sY + y][sX + x] = true;
				colorRecords[sY + y][sX + x] = curColor;
			}
		}

		flag = true;
		for(x = 0; x < WIDTH_SHAPE_NUM; x ++) {
			if(!squareRecords[sY + y][x]){
				flag = false;
				break;
			}
		}
		if(flag) {
			stack[++idx] = sY + y;
		}
	}
	if(idx >= 0) {
		scoreNum += (idx + 1) * 2 - 1;
		lineNum += idx + 1;

		dprintf("stack:");
		for(y = 0; y <= idx; y ++)
			dprintf(" %d", stack[y]);
		dprintf("\n");

		y = stack[idx --];
		size = 1;
		do {
			while(idx >= 0 && y - size == stack[idx]) {
				size++;
				idx --;
			}
			for(x = 0; x < WIDTH_SHAPE_NUM; x ++){
				squareRecords[y][x] = squareRecords[y - size][x];
				colorRecords[y][x] = colorRecords[y - size][x];
			}
			y --;
		} while(y - size >= 0);
		for(; y >= 0; y--) {
			for(x = 0; x < WIDTH_SHAPE_NUM; x++) {
				squareRecords[y][x] = false;
			}
		}
	}
}

void game_next_shape(void) {
	if(endGame) return;
	if(sY < 0) goto end;

	{
		int x, y, mY = 0;
		for(y = 3; y >= 0; y--) {
			for(x = 0; x < 4; x++) {
				if(game_shape_point(nextShape, x, y)) {
					mY = max(mY, y);
					break;
				}
			}
		}
		sX = (WIDTH_SHAPE_NUM - 4) / 2;
		sY = -mY - 1;
	}

	if(!game_movable_shape(nextShape, sX, sY + 1)) {
	end:
		curShape = 0;
		pauseGame = false;
		endGame = true;
		return;
	}
	curShape = nextShape;
	nextShape = SHAPES[rand() % SHAPE_NUM];
	curColor = nextColor;
	nextColor = game_rand_color();
	{
		maxGrade = MAX_GRADE - scoreNum / MAX_GRADE;
		idxGrade = 0;
		if(maxGrade < 1) maxGrade = 1;
		game_timer();
	}
}

// transshape
void game_key_trans(void) {
	int shape;

	if(!beginGame || pauseGame || endGame) return;

	shape = game_rotate_shape(curShape);
	if(game_movable_shape(shape, sX, sY)) {
		curShape = shape;
		game_render();
		fb_sync();
	}
}

// left move
void game_key_left(void) {
	if(pauseGame || endGame) return;

	if(game_movable_shape(curShape, sX - 1, sY)) {
		sX --;
		game_render();
		fb_sync();
	}
}

// right move
void game_key_right(void) {
	if(pauseGame || endGame) return;

	if(game_movable_shape(curShape, sX + 1, sY)) {
		sX ++;
		game_render();
		fb_sync();
	}
}

// down move
void game_key_down(void) {
	if(pauseGame || endGame) return;

	if(game_movable_shape(curShape, sX, sY + 1)) {
		sY ++;
	} else {
		game_save_shape();
		game_next_shape();
	}

	game_render();
	fb_sync();

	idxGrade = 0;
	game_timer();
}

// fall
void game_key_fall(void) {
	if(pauseGame || endGame) return;

	while(game_movable_shape(curShape, sX, sY + 1))
		sY++;

	game_save_shape();
	game_next_shape();

	game_render();
	fb_sync();
}


