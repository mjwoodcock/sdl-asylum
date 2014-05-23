#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

int read_key();
int read_key_unicode();
int read_key_greater_than_15();
void clear_esc_key();
int scan_for_key(int c);

#endif
