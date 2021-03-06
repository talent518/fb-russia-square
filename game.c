#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <signal.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <math.h>
#include <time.h>

#include "fb.h"
#include "api.h"

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

	if(argc >= 2) ret = fb_init(argv[1]);
	else ret = fb_init("/dev/fb0");
	
	if(ret == FB_ERR) return 1;

	signal(SIGPIPE, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if(fb_save() == FB_ERR) eprintf("save failed\n");
	
	init_key();
	
	{
		int flag = fcntl(0, F_GETFL);
		if(flag == -1) {
			eprintf("fcntl F_GETFL");
		} else {
			flag |= O_NONBLOCK;
			if(fcntl(0, F_SETFL, flag) == -1) eprintf("fcntl F_GETFL");
		}
	}

	fprintf(stdout, "\033[?25l"); // hide cursor
	fflush(stdout);

	game_init();
	
	signal(SIGALRM, signal_handler);
	game_timer();

	while(is_running) {
		ret = read_key(1);
		if(ret > 0) {
			game_key(ret);
		} else if(ret < 0) {
			is_running = 0;
		}
	};
	fprintf(stdout, "\033[?25h"); // show cursor
	fflush(stdout);

	restore_key();

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
	switch(key) {
		case 0x1b:
		case 'q':
		case 'Q':
			is_running = 0;
			break;
		case KEY_F1:
		case '[': // start game
			game_start();
			break;
		case KEY_F2:
		case ']': // pause game
			game_pause();
			break;
		case KEY_LEFT:
		case '4':
		case 'a':
		case 'A':
		case 'j':
		case 'J': // left move
			game_key_left();
			break;
		case KEY_RIGHT:
		case '6':
		case 'd':
		case 'D':
		case 'l':
		case 'L': // right move
			game_key_right();
			break;
		case KEY_DOWN:
		case '5':
		case 's':
		case 'S':
		case 'k':
		case 'K': // down move
			game_key_down();
			break;
		case KEY_UP:
		case '8':
		case 'w':
		case 'W':
		case 'i':
		case 'I': // transshape
			game_key_trans();
			break;
		case ' ':
		case '0':
			game_key_fall();
			break;
		default:
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
	"      HELP    ",
	"--------------",
	"Start: [ F1",
	"Pause: ] F2",
	"Trans: 8 w i Up",
	" Left: 4 a j Left",
	"Right: 6 d l Right",
	" Down: 5 s k Down",
	" Fall: 0 Space",
	" Quit: q Q ESC"
};
const int HELPLEN = sizeof(HELPS) / sizeof(HELPS[0]);

static int sX, sY, scoreNum, lineNum;
static bool squareRecords[HEIGHT_SHAPE_NUM][WIDTH_SHAPE_NUM];
static int colorRecords[HEIGHT_SHAPE_NUM][WIDTH_SHAPE_NUM];
static int curShape, nextShape = 0;
static int curColor, nextColor = 0;
static bool beginGame, endGame, pauseGame;
static int maxGrade = MAX_GRADE;
static int idxGrade = 0;
static int overColor = 0;
static int overOffset = 0;
static int overStep = 1;

void game_alrm(void) {
	if(beginGame && (endGame || pauseGame)) {
		PROF(game_render);
	} else {
		if(++idxGrade >= maxGrade) {
			idxGrade = 0;
			if(!beginGame || pauseGame || endGame) PROF(game_render);
			else PROF(game_key_down);
		}
	}
	fb_sync();
}

int game_rand_color() {
	int r, g, b;
	r = rand() % COLOR_NUM;
	g = rand() % COLOR_NUM;
	b = rand() % COLOR_NUM;
	return fb_color(COLORS[r], COLORS[g], COLORS[b]);
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
	if(nextShape == 0) nextShape = SHAPES[rand() % SHAPE_NUM];
	if(nextColor == 0) nextColor = game_rand_color();
	scoreNum = lineNum = 0;
	for(y = 0; y < HEIGHT_SHAPE_NUM; y ++) {
		for(x = 0; x < WIDTH_SHAPE_NUM; x ++) {
			squareRecords[y][x] = false;
			colorRecords[y][x] = 0;
		}
	}

	overColor = overOffset = 0;
	overStep = 1;
}

void game_init(void) {
	game_reset();
	game_render();
	fb_sync();
}

void game_next_shape(void);
void game_start(void) {
	if(beginGame) game_reset();
	beginGame = true;
	game_next_shape();
	game_render();
}

void game_pause(void) {
	if(!beginGame) return;

	pauseGame = !pauseGame;
	idxGrade = 0;

	overColor = overOffset = 0;
	overStep = 1;

	game_render();
	fb_sync();

	game_timer();
}

void game_draw(int x, int y, int side, int color) {
	int addcolor = fb_color_add(color, 0x33);
	int subcolor = fb_color_add(color, -0x33);
#if 0
	fb_draw_line(x, y, x + side - 1, y, addcolor, 1);
	fb_draw_line(x, y, x, y + side - 1, addcolor, 1);
	fb_draw_line(x + side - 1, y, x + side - 1, y + side - 1, subcolor, 1);
	fb_draw_line(x, y + side - 1, x + side - 1, y + side - 1, subcolor, 1);
#else
	fb_fill_rect(x, y, 1, side, addcolor);
	fb_fill_rect(x, y, side, 1, addcolor);
	fb_fill_rect(x + side - 1, y, 1, side, subcolor);
	fb_fill_rect(x, y + side - 1, side, 1, subcolor);
#endif
	fb_fill_rect(x + 1, y + 1, side - 2, side - 2, color);
}

bool game_shape_point(int p, int x, int y) {
	return (p & (1 << (16 - (x + 1 + y * 4))));
}

typedef struct {
	float real;
	float imag;
} complex;

int cal_pixel(complex c) {
	int count;
	complex z;
	float temp, lengthsq;
	z.real = 0;
	z.imag = 0;
	count = 0;
	do {
		temp = z.real * z.real - z.imag * z.imag + c.real;
		z.imag = 2 * z.real * z.imag + c.imag;
		z.real = temp;
		lengthsq = z.real * z.real + z.imag * z.imag;
		count++;
	} while ((lengthsq < 4.0) && (count < 291));

	return count - 0x24;
}

static bool is_help = true;
void game_render(void) {
	const int side = fb_height / (HEIGHT_SHAPE_NUM + 2);
	const int X = (fb_width - (WIDTH_SHAPE_NUM + 5) * side) / 2, Y = (fb_height - HEIGHT_SHAPE_NUM * side) / 2;
	const int X2 = X + (WIDTH_SHAPE_NUM + 1) * side;
	const int bdcolor = 0xff666666;
	int Y2 = Y;
	int x, y;
	int x2, y2;

	fb_draw_rect(X - 3, Y - 3, WIDTH_SHAPE_NUM * side + 6, HEIGHT_SHAPE_NUM * side + 6, bdcolor, 1);

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

	fb_draw_rect(X2 - 3, Y2 - 3, 4 * side + 6, 4 * side + 6, bdcolor, 1);

	for(y = 0; y < 4; y ++) {
		for(x = 0; x < 4; x ++) {
			// draw next shape
			x2 = X2 + x * side;
			y2 = Y2 + y * side;
			if(game_shape_point(nextShape, x, y)) {
				game_draw(x2, y2, side, nextColor);
			} else {
				fb_fill_rect(x2, y2, side, side, 0xffffffff);
			}
		}
	}
	
	Y2 += 4 * side;

	fb_set_font(FONT_12x22);
	
	// draw scores, lines and grade
	{
		char str[20];
		Y2 += side;
		
		fb_draw_rect(X2 - 3, Y2 - 3, 4 * side + 6, fb_font_height() * 4.0f + 6, bdcolor, 1);

		Y2 += fb_font_height() * 0.2f;

		sprintf(str, "%d", scoreNum);
		fb_fill_rect(X2, Y2, 4 * side, fb_font_height(), 0);
		fb_text(X2 + 3, Y2, "SCORE:", 0xffcccccc, 0, 1);
		fb_text(X2 + fb_font_width() * 7, Y2, str, fb_color(0xff, 0x66, 0), 1, 1);

		Y2 += fb_font_height() * 1.2f;
		fb_fill_rect(X2 - 3, Y2 - 1, 4 * side + 6, 1, bdcolor);
		Y2 += fb_font_height() * 0.2f;

		sprintf(str, "%d", lineNum);
		fb_fill_rect(X2, Y2, 4 * side, fb_font_height() - 1, 0);
		fb_text(X2 + 3, Y2, " LINE:", 0xffcccccc, 0, 1);
		fb_text(X2 + fb_font_width() * 7, Y2, str, fb_color(0xff, 0x33, 0), 1, 1);

		Y2 += fb_font_height() * 1.2f;
		fb_fill_rect(X2 - 3, Y2 - 1, 4 * side + 6, 1, bdcolor);
		Y2 += fb_font_height() * 0.2f;

		sprintf(str, "%d", MAX_GRADE + 1 - maxGrade);
		fb_fill_rect(X2, Y2, 4 * side, fb_font_height() - 1, 0);
		fb_text(X2 + 3, Y2, "GRADE:", 0xffcccccc, 0, 1);
		fb_text(X2 + fb_font_width() * 7, Y2, str, fb_color(0xff, 0x33, 0), 1, 1);

		Y2 += fb_font_height() * 1.2f;
	}

	fb_set_font(FONT_08x14);
	
	// draw help
	if(is_help) {
		int i;
		int fh = fb_font_height() + 1;
		int gray = fb_color(0x99, 0x99, 0x99);
		int slen = 0;

		is_help = false;
		
		for(i = 0; i < HELPLEN; i ++) {
			x2 = strlen(HELPS[i]);
			if(x2 > slen) slen = x2;
		}

		x2 = slen * fb_font_width();
		y2 = HELPLEN * fh;
		x = X2 + 4 * side - x2;
		y = Y2 + (Y + (HEIGHT_SHAPE_NUM - 5) * side - Y2 - y2) / 2;
		
		fb_draw_rect(x - 3, y - 3, x2 + 6, y2 + 6, bdcolor, 1);

		for(i = 0; i < HELPLEN; i ++) fb_text(x, y + i * fh, HELPS[i], gray, 0, 1);
		
		// Mandelbrot set
		{
		#define mset 2 // recommand 0,2,4
		#if mset == 1
			float real_min = -0.84950f, imag_min = 0.21000f;
			float real_max = -0.84860f, imag_max = 0.21090f;
			#define mcolor color
		#elif mset == 2
			float real_min = 0.32000f, imag_min = -0.45000f;
			float real_max = 0.50000f, imag_max = 0.05000f;
			#define mcolor 0xff - color
		#elif mset == 3
			float real_min = 0.26304f, imag_min = 0.00233f;
			float real_max = 0.26329f, imag_max = 0.00267f;
			#define mcolor color
		#elif mset == 4
			float real_min = -0.63500f, imag_min = 0.68000f;
			float real_max = -0.62500f, imag_max = 0.69000f;
			#define mcolor 0xff - color
		#elif mset == 5
			float real_min = -0.46510f, imag_min = -0.56500f;
			float real_max = -0.46470f, imag_max = -0.56460f;
			#define mcolor 0xff - color
		#else
			float real_min = -1.50000f, imag_min = -1.00000f;
			float real_max = 0.50000f, imag_max = 1.00000f;
			#define mcolor 0xff - color
		#endif
		#undef mset
			int w = X - Y / 2;
			int x2 = fb_width - w;
			complex c;
			float scale_real = (real_max - real_min) / w;
			float scale_imag = (imag_max - imag_min) / fb_height;
			int r, g, b;
			int color;

			for(y = 0; y < fb_height; y ++) {
				c.imag = imag_min + ((float) y * scale_imag);
				r = ((int) ((y + 1) * 255.0f / (float) fb_height)) & 0xff;
				for(x = 0; x < w; x ++) {
					c.real = real_min + ((float) x * scale_real);
					color = cal_pixel(c);

					g = ((int) ((x + 1) * 255.0f / (float) w)) & 0xff;
					b = mcolor;
					// printf("%06x\n", b);

					color = fb_color(b, g, r);
					fb_draw_point(x, y, color);
					fb_draw_point(x2 + w - 1 - x, y, color);
				}
			}
		#undef mcolor
		}
		
		// Gradual change: vertical
		{
			int r1 = 0xff, g1 = 0, b1 = 0;
			int r2 = 0, g2 = 0xff, b2 = 0;
			int w = Y / 2;
			float rr = (float) (r2 - r1) / (float) fb_height, gg = (float) (g2 - g1) / (float) fb_height, bb = (float) (b2 - b1) / (float) fb_height;
			float r, g, b;
			int i;

			for(i = 0, r = r1, g = g1, b = b1; i < fb_height; i ++, r += rr, g += gg, b += bb) {
				fb_fill_rect(X - Y, i, w, 1, fb_color(r, g, b));
				fb_fill_rect(fb_width - X - 1 + w, i, w, 1, fb_color(g, b, r));
			}
		}

		// Gradual change: horizontal
		{
			int r1 = 0xff, g1 = 0, b1 = 0;
			int r2 = 0, g2 = 0, b2 = 0xff;
			int h = Y / 2;
			int w = 2 * h + (WIDTH_SHAPE_NUM + 5) * side;
			float rr = (float) (r2 - r1) / (float) w, gg = (float) (g2 - g1) / (float) w, bb = (float) (b2 - b1) / (float) w;
			float r, g, b;
			int i;

			for(i = 0, r = r1, g = g1, b = b1; i < w; i ++, r += rr, g += gg, b += bb) {
				fb_fill_rect(X - h + i, 0, 1, h, fb_color(r, g, b));
				fb_fill_rect(X - h + w - 1 - i, fb_height - h, 1, h, fb_color(r, b, g));
			}
		}
	}

	fb_set_font(FONT_12x22);
	
	Y2 = Y + (HEIGHT_SHAPE_NUM - 5) * side;
	
	fb_draw_rect(X2 - 3, Y2 - 3, 4 * side + 6, 5 * side + 6, bdcolor, 1);
	
	// draw time
	{
		const time_t t = time(NULL);
		struct tm tm;
		const int radius = side * 2;
		const int x0 = X2 + 2 * side, y0 = Y2 + 3 * side;
		double angle;
		int weight;

		localtime_r(&t, &tm);
		
		{
			char str[10];
			sprintf(str, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
			x = X2 + (4 * side - fb_font_width() * 8) / 2;
			y = Y2 + (side - fb_font_height()) / 2;
			fb_fill_rect(X2, Y2, 4 * side, side, 0);
			fb_text(x, y, str, 0xffffffff, 0, 1);

			fb_draw_rect(X2, Y2, 4 * side, side + 1, 0xffffffff, 1);
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
			fb_draw_line(x0, y0, x, y, fb_color(0, 0, 0xff), 5);
		}

		// minute
		{
			angle = (90.0f - tm.tm_min * 6.0f - tm.tm_sec / 10.0f) * M_PI / 180.0f;
			x = radius * 0.6f * cos(angle) + x0;
			y = -radius * 0.55f * sin(angle) + y0;
			fb_draw_line(x0, y0, x, y, fb_color(0, 0xff, 0), 3);
		}

		// second
		{
			angle = (90.0f - tm.tm_sec * 6) * M_PI / 180.0f;
			x = radius * 0.75f * cos(angle) + x0;
			y = -radius * 0.75f * sin(angle) + y0;
			fb_draw_line(x0, y0, x, y, fb_color(0xff, 0, 0), 1);
		}
	}
	
	fb_set_font(FONT_18x32);
	if(beginGame && (endGame || pauseGame)) {
		const char *str = endGame ? "OVER!" : "PAUSE";
		const int sz = (WIDTH_SHAPE_NUM * side) / (fb_font_width() * strlen(str));
		const int offset = (HEIGHT_SHAPE_NUM * side - fb_font_height() * sz) / 2;
		const int step = offset / 1.5f / MAX_GRADE;

		if(overOffset == 0) overColor = game_rand_color();

		x = X + (WIDTH_SHAPE_NUM * side - fb_font_width() * sz * strlen(str)) / 2;
		y = Y + offset + overOffset;
		fb_text(x - 1, y - 1, str, fb_color_add(overColor, 0x33), 1, sz);
		fb_text(x + 1, y + 1, str, fb_color_add(overColor, -0x33), 1, sz);
		fb_text(x, y, str, overColor, 1, sz);
		
		overOffset += step * overStep;
		if(overStep > 0) {
			if(overOffset > offset) {
				overStep = -1;
				overOffset += step * overStep;
			}
		} else {
			if(overOffset < -offset) {
				overStep = 1;
				overOffset += step * overStep;
			}
		}
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

		overColor = overOffset = 0;
		overStep = 1;

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
	if(!beginGame || pauseGame || endGame) return;

	if(game_movable_shape(curShape, sX - 1, sY)) {
		sX --;
		game_render();
		fb_sync();
	}
}

// right move
void game_key_right(void) {
	if(!beginGame || pauseGame || endGame) return;

	if(game_movable_shape(curShape, sX + 1, sY)) {
		sX ++;
		game_render();
		fb_sync();
	}
}

// down move
void game_key_down(void) {
	if(!beginGame || pauseGame || endGame) return;

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
	if(!beginGame || pauseGame || endGame) return;

	while(game_movable_shape(curShape, sX, sY + 1))
		sY++;

	game_save_shape();
	game_next_shape();

	game_render();
	fb_sync();
}


