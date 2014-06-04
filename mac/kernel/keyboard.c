/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            keyboard.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*modified by luck,2014
 * based on orange
 * */
 
 
#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "keyboard.h"
#include "keymap.h"

PRIVATE KB_INPUT	kb_in;

/*
 * only these states is useful
 * */
PRIVATE	int	code_with_E0;
PRIVATE	int	right_shift;	
PRIVATE	int	left_shift;	
PRIVATE	int	caps_lock;	
PRIVATE	int	column;

PRIVATE u8	get_byte_from_kbuf();
PRIVATE void    kb_wait();
PRIVATE void    kb_ack();

/*======================================================================*
                            keyboard_handler
 *======================================================================*/
PUBLIC void keyboard_handler(int irq)
{
	u8 scan_code = in_byte(KB_DATA);

	if (kb_in.count < KB_IN_BYTES) {
		*(kb_in.p_head) = scan_code;
		kb_in.p_head++;
		if (kb_in.p_head == kb_in.buf + KB_IN_BYTES) {
			kb_in.p_head = kb_in.buf;
		}
		kb_in.count++;
	}
}


/*======================================================================*
                           init_keyboard
*======================================================================*/
PUBLIC void init_keyboard()
{
	
	/*init the kb_in */
	kb_in.count = 0;
	kb_in.p_head = kb_in.p_tail = kb_in.buf;

	code_with_E0=0;
	left_shift=right_shift=0;
	caps_lock=0;
	
	
    put_irq_handler(KEYBOARD_IRQ, keyboard_handler);/*init the keyboard interrupt*/
    enable_irq(KEYBOARD_IRQ);                       /*open the keyboard interrupt*/
}


/*======================================================================*
                           keyboard_read
*======================================================================*/
PUBLIC void keyboard_read(TTY* p_tty)
{
		u8	scan_code;
	char	output[2];
	int	make;	/* 1: make;  0: break. */

	u32	key = 0;
	u32* keyrow;	/* locate the row in the keymap */

	if(kb_in.count > 0){
		code_with_E0 = 0;

		scan_code = get_byte_from_kbuf();

		/* scan the code */
		if (scan_code == 0xE1) {
			/* ignore do nothing */
		}
		else if (scan_code == 0xE0) {
			/* have to judge this in order to judege the pad num */
			scan_code = get_byte_from_kbuf();

			/* PrintScreen keyPressed*/
			if (scan_code == 0x2A) {
				if (get_byte_from_kbuf() == 0xE0) {
					if (get_byte_from_kbuf() == 0x37) {
						key = PRINTSCREEN;
						make = 1;
					}
				}
			}
			/* PrintScreen Keyreleased*/
			if (scan_code == 0xB7) {
				if (get_byte_from_kbuf() == 0xE0) {
					if (get_byte_from_kbuf() == 0xAA) {
						key = PRINTSCREEN;
						make = 0;
					}
				}
			}
			/* the scan_code is the value followed by it. */
			if (key == 0) {
				code_with_E0 = 1;
			}
		}
		if ((key != PAUSEBREAK) && (key != PRINTSCREEN)) {
			/* judge the make or break */
			make = (scan_code & FLAG_BREAK ? 0 : 1);

			/* locate the row */
			keyrow = &keymap[(scan_code & 0x7F) * MAP_COLS];
			column = 0;
			
			/* if shift */
			int caps = left_shift || right_shift;
			
			if (caps_lock) {
				if ((keyrow[0] >= 'a') && (keyrow[0] <= 'z')){
					caps = !caps;
				}
			}
			if (caps) {
				column = 1;
			}

			if (code_with_E0) {
				column = 2;
			}

			key = keyrow[column];

			switch(key) {
			case SHIFT_L:
				right_shift = make;
				break;
			case SHIFT_R:
				left_shift = make;
				break;
			case CTRL_L:
				make = 0;
				break;
			case CTRL_R:
				make = 0;
				break;
			case ALT_L:
				make=0;
				break;
			case ALT_R:
				make=0;
				break;
			case CAPS_LOCK:
				if (make) {
					caps_lock   = !caps_lock;
				}
				break;
			case NUM_LOCK:
				make = 0;
				break;
			case SCROLL_LOCK:
				make=0;
				break;
			default:
				break;
			}

			if (make) { /* ignore Break Code */
				key |= left_shift	? FLAG_SHIFT_L	: 0;
				key |= right_shift	? FLAG_SHIFT_R	: 0;
				
				in_process(p_tty, key);
			}
		}
	}
}

/*======================================================================*
			    get_byte_from_kbuf
 *======================================================================*/
PRIVATE u8 get_byte_from_kbuf()       /* 从键盘缓冲区中读取下一个字节 */
{
        u8 scan_code;

        while (kb_in.count <= 0) {}   /* 等待下一个字节到来 */

        disable_int();
        scan_code = *(kb_in.p_tail);
        kb_in.p_tail++;
        if (kb_in.p_tail == kb_in.buf + KB_IN_BYTES) {
                kb_in.p_tail = kb_in.buf;
        }
        kb_in.count--;
        enable_int();

	return scan_code;
}

/*======================================================================*
				 kb_wait
 *======================================================================*/
PRIVATE void kb_wait()	/* 等待 8042 的输入缓冲区空 */
{
	u8 kb_stat;

	do {
		kb_stat = in_byte(KB_CMD);
	} while (kb_stat & 0x02);
}


/*======================================================================*
				 kb_ack
 *======================================================================*/
PRIVATE void kb_ack()
{
	u8 kb_read;

	do {
		kb_read = in_byte(KB_DATA);
	} while (kb_read =! KB_ACK);
}


