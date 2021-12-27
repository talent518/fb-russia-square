#include "api.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

static struct termios newset, oldset;
static int flags;

void init_key() {
	tcgetattr(0, &oldset);
	memcpy(&newset, &oldset, sizeof(oldset));
	newset.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHOPRT|ECHOKE|ICRNL);
	newset.c_cc[VTIME] = 0;
	newset.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &newset);

	if(!fcntl(0, F_GETFL, &flags)) {
		fcntl(0, F_SETFL, flags | O_NONBLOCK);
	}
}

void restore_key() {
	tcsetattr(0, TCSANOW, &oldset);
	fcntl(0, F_SETFL, flags);
}

int read_key(int secs) {
	struct timeval tv;
	fd_set set;
	unsigned char buf[16];
	int ret;

	FD_ZERO(&set);
	FD_SET(0, &set); // 0 => stdin

	tv.tv_sec = secs;
	tv.tv_usec = 0;

	ret = select(1, &set, NULL, NULL, &tv);
	if(ret == 0) {
		return 0;
	} else if(ret < 0) {
		if(errno == EINTR || errno == EINVAL) {
			return 0;
		} else {
			return -errno;
		}
	}

	ret = read(0, buf, sizeof(buf));
	if(ret <= 0) {
		return -errno;
	}

#ifdef DEBUG_KEY
	dprintf(2, "%d\033[31m", ret);
	for(int i=0; i < ret; i++) {
		if(isprint(buf[i])) {
			dprintf(2, " %c", buf[i]);
		} else {
			dprintf(2, " %02x", buf[i]);
		}
	}
	dprintf(2, "\033[0m\n");
#endif

	switch(ret) {
		case 1:
			return buf[0];
		case 2: // alt + char
			if(buf[0] != 0x1b) return KEY_IGNORE;

			return (buf[1] & 0xff) | 0x100;
		case 3:
			if(buf[0] != 0x1b) return KEY_IGNORE;
			
			switch(buf[1]) {
				case 'O':
					switch(buf[2]) {
						case 'P':
							return KEY_F1;
						case 'Q':
							return KEY_F2;
						case 'R':
							return KEY_F3;
						case 'S':
							return KEY_F4;
						default:
							return KEY_IGNORE;
					}
					break;
				case '[':
					switch(buf[2]) {
						case 'A':
							return KEY_UP;
						case 'B':
							return KEY_DOWN;
						case 'C':
							return KEY_RIGHT;
						case 'D':
							return KEY_LEFT;
						case 'Z':
							return KEY_SHIFT_TAB;
						default:
							return KEY_IGNORE;
					}
					break;
				default:
					return KEY_IGNORE;
			}
		case 4:
		case 5:
			if(buf[0] != 0x1b || buf[1] != '[') return KEY_IGNORE;
			
			switch(buf[2]) {
				case '[':
					switch(buf[3]) {
						case 'A':
							return KEY_F1;
						case 'B':
							return KEY_F2;
						case 'C':
							return KEY_F3;
						case 'D':
							return KEY_F4;
						case 'E':
							return KEY_F5;
						default:
							return KEY_IGNORE;
					}
				case '1':
					if(ret == 5 && buf[4] != '~') return KEY_IGNORE;

					switch(buf[3]) {
						case '7':
							return KEY_F6;
						case '8':
							return KEY_F7;
						case '9':
							return KEY_F8;
						default:
							return KEY_IGNORE;
					}
				case '2':
					if(ret == 5 && buf[4] != '~') return KEY_IGNORE;

					switch(buf[3]) {
						case '0':
							return KEY_F9;
						case '1':
							return KEY_F10;
						case '3':
							return KEY_F11;
						case '4':
							return KEY_F12;
						default:
							return KEY_IGNORE;
					}
				default:
					return KEY_IGNORE;
			}
		default:
			return KEY_IGNORE;
	}
}

