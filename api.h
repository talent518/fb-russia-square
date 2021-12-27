#ifndef _API_H
#define _API_H

#define KEY_SHIFT_TAB 0xfe
#define KEY_IGNORE 0xff
#define KEY_ALT 0x100
#define KEY_ALT_MAX 0x1ff
#define KEY_LEFT 0x200
#define KEY_RIGHT 0x300
#define KEY_UP 0x400
#define KEY_DOWN 0x500
#define KEY_F1 0x600
#define KEY_F2 0x700
#define KEY_F3 0x800
#define KEY_F4 0x900
#define KEY_F5 0xa00
#define KEY_F6 0xb00
#define KEY_F7 0xc00
#define KEY_F8 0xd00
#define KEY_F9 0xe00
#define KEY_F10 0xf00
#define KEY_F11 0x1000
#define KEY_F12 0x1100

void init_key();
void restore_key();
int read_key(int secs);

#endif
