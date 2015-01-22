/* test_lcd-linux.c
 *
 * $Id: test_lcd-linux.c,v 1.76 2010/07/17 19:08:24 mjona Exp $
 *
 * Lcd-linux debugger.
 *
 * Copyright (C) 2005 - 2010  Mattia Jona-Lasinio (mjona@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 only,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct lcd_parameters {
	const char	*name;		/* Driver's name */
	unsigned long	flags;		/* Flags (see documentation) */
	unsigned short	minor;		/* Minor number of the char device */
	unsigned short	tabstop;	/* Tab character length */
	unsigned short	num_cntr;	/* Controllers to drive */
	unsigned short	cntr_rows;	/* Rows per controller */
	unsigned short	cntr_cols;	/* Display columns */
	unsigned short	vs_rows;	/* Virtual screen rows */
	unsigned short	vs_cols;	/* Virtual screen columns */
	unsigned short	cgram_chars;	/* Number of user definable characters */
	unsigned short	cgram_bytes;	/* Number of bytes required to define a
					 * user definable character */
	unsigned char	cgram_char0;	/* Ascii of first user definable character */
};

struct lcd_driver {
	void	(*read_char)(unsigned int offset, unsigned short *data);
	void	(*read_cgram_char)(unsigned char index, unsigned char *pixmap);
	void	(*write_char)(unsigned int offset, unsigned short data);
	void	(*write_cgram_char)(unsigned char index, unsigned char *pixmap);
	void	(*clear_display)(void);
	void	(*address_mode)(int mode);
	int	(*validate_driver)(void);
	int	(*init_display)(void);
	int	(*cleanup_display)(void);
	int	(*init_port)(void);
	int	(*cleanup_port)(void);
	int	(*handle_custom_char)(unsigned int data);
	int	(*handle_custom_ioctl)(unsigned int cmd, unsigned long arg, unsigned int arg_in_userspace);

	/* The character map to be used */
	unsigned char *charmap;
};

int lcd_register_driver(struct lcd_driver *, struct lcd_parameters *);
int lcd_unregister_driver(struct lcd_driver *, struct lcd_parameters *);

/* asm/bitops.h substitutes */
#define set_bit(n, p)	(*(p) |= (1 << n))
#define clear_bit(n, p)	(*(p) &= ~(1 << n))
#define test_bit(n, p)	(*(p) & (1 << n))

static inline void __set_br(unsigned long *x, unsigned long val, unsigned long start, unsigned long len)
{
	unsigned long mask = (1 << len)-1;

	if (val <= mask) {
		mask <<= start;
		val <<= start;
		*x = (*x & ~mask) | val;
	}
}

static inline unsigned long __get_br(unsigned long x, unsigned long start, unsigned long len)
{
	return ((x >> start) & ((1 << len)-1));
}

/*** struct_flags ***/

/* internal flags: bits [0:15] of struct_flags (16 flags allowed) */
#define NEED_WRAP	0		/* Next char will trigger a newline */
#define DECIM		1		/* Insert mode */
#define DECOM		2		/* Row origin of cursor: absolute or relative to scrolling region */
#define DECAWM		3		/* Autowrap */
#define DECSCNM		4		/* Inverted screen */
#define CRLF		5		/* Follow lf, vt, ff, with a cr */
#define INC_CURS_POS	6		/* Increment cursor position after data read/write */
#define QUES		7		/* CSI Esc sequence contains a question mark */
#define USER_SPACE	8		/* If set, the buffer pointed by arg in do_lcd_ioctl() is
					 * assumed to be in user space otherwise it is in kernel space */
#define NULL_CHARMAP	9		/* The driver doesn't provide a charmap so the
					 * lcd-linux layer provides one*/
#define CAN_DO_COLOR	10		/* The display is color capable */
#define WITH_ATTR	11		/* If set, the void * buffer in do_lcd_read/write() contains
					 * attributes and therefore is an unsigned short * otherwise it
					 * is an unsigned char *
					 */

/* input states: bits [24:27] of struct_flags (16 states allowed) */
#define NORMAL		0	/* Normal mode */
#define RAW		1	/* Raw mode (console emulation disabled) */
#define SYN		2	/* Synchronous Idle mode */
#define ESC		3	/* Escape mode */
#define CSI		4	/* CSI escape mode */
#define ESC_G0		5	/* G0 character set */
#define ESC_G1		6	/* G1 character set */
#define ESC_HASH	7	/* ESC # escape sequence */
#define ESC_PERCENT	8	/* ESC % escape sequence */
#define ARG		9	/* Waiting for arguments for the lcd-linux layer */
#define ARG_DRIVER 	10	/* Waiting for arguments for the display driver */

#define SET_ESC_STATE(p, x)	__set_br(&(p)->struct_flags, x, 16, 8)
#define SET_INPUT_STATE(p, x)	__set_br(&(p)->struct_flags, x, 24, 4)
#define SET_INIT_LEVEL(p, x)	__set_br(&(p)->struct_flags, x, 28, 2)
#define SET_PROC_LEVEL(p, x)	__set_br(&(p)->struct_flags, x, 30, 2)
#define ESC_STATE(p)		__get_br((p)->struct_flags, 16, 8)
#define INPUT_STATE(p)		__get_br((p)->struct_flags, 24, 4)
#define INIT_LEVEL(p)		__get_br((p)->struct_flags, 28, 2)
#define PROC_LEVEL(p)		__get_br((p)->struct_flags, 30, 2)

/*** attributes ***/
#define I_MASK		0x03		/* Intensity (0 = low, 1 = normal, 2 = bright) */
#define ULINE		0x04		/* Underlined text */
#define	REVERSE		0x08		/* Reversed video text */
#define BLINK		0x80		/* Blinking text */

/*** Color attributes ***/
#define FG_COLOR	0x07				/* Foreground color mask */
#define FG_BRIGHT	0x08				/* Foreground bright color */
#define FG_MASK		(FG_BRIGHT | FG_COLOR)		/* Foreground mask */
#define BG_COLOR	0x70				/* Background color mask */
#define BG_BRIGHT	0x80				/* Background bright color */
#define BG_MASK		(BG_BRIGHT | BG_COLOR)		/* Background mask */

#define NPAR	16			/* Max number of parameters in CSI escape sequence */
#define FLIP_BUF_SIZE	(1 << 6)	/* Flip buffer size (64 bytes) MUST be a power of 2 */

struct lcd_struct {
	struct lcd_driver	*driver;		/* The driver associated to this struct */
	struct lcd_parameters	*par;			/* The parameters associated to this struct */
	unsigned long		struct_flags;		/* Flags for internal use only */
	unsigned int		refcount;		/* Number of references to this struct */

	unsigned short		*display;		/* The display buffer */

	unsigned short		*fb;			/* The virtual screen framebuffer */
	unsigned int		fb_size;		/* Size of the framebuffer */
	unsigned int		frame_base;		/* Offset of row 0, column 0 of a frame in fb */
	unsigned int		frame_size;		/* Size of the frame */

	unsigned int		row;			/* Current row in virtual screen */
	unsigned int		col;			/* Current column in virtual screen */
	unsigned int		s_offset;		/* Saved cursor position in virtual screen */

	unsigned int		top;			/* Top scroll row in virtual screen */
	unsigned int		bot;			/* Bottom scroll row in virtual screen */

	int			esc_args;		/* Number of arguments for a normal escape sequence */
	unsigned int		csi_args[NPAR];		/* CSI parameters */
	unsigned int		index;			/* Index in csi_args and counter for cgram characters generation */
	unsigned char		cgram_index;		/* Index of the cgram character to be created */
	unsigned char		*cgram_buffer;		/* Buffer for cgram operations in this driver */

	unsigned short		erase_char;		/* Character to be used when erasing */
	unsigned char		attr;			/* Current attributes */
	unsigned char		color;			/* Color for normal intensity mode */
	unsigned char		s_color;		/* Saved color for normal intensity mode */
	unsigned char		defcolor;		/* Default color for normal intensity mode */
	unsigned char		ulcolor;		/* Color for underline mode */
	unsigned char		halfcolor;		/* Color for low intensity mode */
	unsigned char		attributes;		/* Packed attributes */
	unsigned char		s_attributes;		/* Saved packed attributes */

	unsigned char		*s_charmap;		/* Saved character map for this driver */
	unsigned char		*flip_buf;		/* High speed flip buffer */
};

/** Function prototypes **/

/* Init/Cleanup the driver */
static int init_driver(struct lcd_struct *);
static int cleanup_driver(struct lcd_struct *);

/* Read from/Write to the driver */
static void write_data(struct lcd_struct *, unsigned short);
static void write_cgram(struct lcd_struct *, unsigned char, unsigned char *);

/* Input handlers */
static void cr(struct lcd_struct *);
static void lf(struct lcd_struct *);
static void control_char(struct lcd_struct *, unsigned char);
static void handle_csi(struct lcd_struct *, unsigned char);
static int handle_custom_esc(struct lcd_struct *, unsigned int);
static int handle_esc(struct lcd_struct *, unsigned char);
static void handle_input(struct lcd_struct *, unsigned short);

static struct lcd_struct *lcd_drivers;

/* Macros for iterator handling */
#define iterator_inc(iterator, module)		(iterator = iterator_inc_(iterator, module))
#define iterator_dec(iterator, module)		(iterator = iterator_dec_(iterator, module))

static inline unsigned int iterator_inc_(unsigned int iterator, const unsigned int module)
{
	return ((++iterator)%module);
}

static inline unsigned int iterator_dec_(unsigned int iterator, const unsigned int module)
{
	return (iterator ? --iterator : module-1);
}

#ifdef DEBUG
#define enter_func						\
static int _count, _i;						\
for (_i = 0; _i < _count; ++_i) printf("\t"); ++_count;		\
printf("start %s\n", __FUNCTION__);
#define exit_func						\
--_count; for (_i = 0; _i < _count; ++_i) printf("\t");		\
printf("end   %s\n", __FUNCTION__);
#else
#define enter_func
#define exit_func
#endif

/************************************
 * Low level routines and utilities *
 ************************************/
/*
 * Set whether the address counter should be incremented
 * or decremented after a Read/Write
 */
static void address_mode(struct lcd_struct *p, int mode)
{
	struct lcd_driver *driver = p->driver;

enter_func
	if (mode > 0 && ! test_bit(INC_CURS_POS, &p->struct_flags)) {
		if (driver->address_mode)
			driver->address_mode(mode);
		set_bit(INC_CURS_POS, &p->struct_flags);
	} else if (mode < 0 && test_bit(INC_CURS_POS, &p->struct_flags)) {
		if (driver->address_mode)
			driver->address_mode(mode);
		clear_bit(INC_CURS_POS, &p->struct_flags);
	}
exit_func
}

/* WARNING!! This function returns an int because if iterator is not
 * within the visible area of the frame it returns -1
 */
static inline int vs_to_frame_(struct lcd_struct *p, unsigned int iterator)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int row = iterator/vs_cols;
	unsigned int col = iterator%vs_cols;
	unsigned int frame_base_row = p->frame_base/vs_cols;
	unsigned int frame_base_col = p->frame_base%vs_cols;
	unsigned int frame_rows = p->par->cntr_rows*p->par->num_cntr;
	unsigned int frame_cols = p->par->cntr_cols;

	if (vs_rows == frame_rows && vs_cols == frame_cols)
		return (iterator);

	if (row < frame_base_row || row >= frame_base_row+frame_rows)
		return (-1);
	if (col < frame_base_col || col >= frame_base_col+frame_cols)
		return (-1);

	return ((row-frame_base_row)*frame_cols+(col-frame_base_col));
}

/* Given 'iterator' in vs, returns the offset in vs corresponding to the nearest
 * visible offset in vs, or returns 'iterator' if it is already visible.
 */
static unsigned int round_vs_(struct lcd_struct *p, unsigned int iterator)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int row = iterator/vs_cols;
	unsigned int col = iterator%vs_cols;
	unsigned int frame_base_row = p->frame_base/vs_cols;
	unsigned int frame_base_col = p->frame_base%vs_cols;
	unsigned int frame_rows = p->par->cntr_rows*p->par->num_cntr;
	unsigned int frame_cols = p->par->cntr_cols;

	if (vs_rows == frame_rows && vs_cols == frame_cols)
		return (iterator);

	if (row < frame_base_row)
		row = frame_base_row;
	else if (row >= frame_base_row+frame_rows)
		row = frame_base_row+(frame_rows-1);

	if (col < frame_base_col)
		col = frame_base_col;
	else if (col >= frame_base_col+frame_cols)
		col = frame_base_col+(frame_cols-1);

	return ((row*vs_cols)+col);
}

#define round_vs(p, iterator)			(iterator = round_vs_(p, iterator))

/*
 * Sync the frame area starting at offset s, ending at offset e with fb content.
 */
static void redraw_screen(struct lcd_struct *p, unsigned int s, unsigned int e)
{
	unsigned int len;
	unsigned int row = p->row, col = p->col;
	unsigned int inc_set = test_bit(INC_CURS_POS, &p->struct_flags);
	unsigned int frame_cols = p->par->cntr_cols;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned long flags;

enter_func
	if (s >= p->fb_size || e >= p->fb_size || e < s || e < p->frame_base)
		return;

	round_vs(p, s);
	round_vs(p, e);

	len = 1+e-s;

	if (! inc_set)
		s = e;

#ifdef DEBUG
for (_i = 0; _i < _count-1; ++_i) printf("\t"); printf(" cursor at (%d, %d)\ts=%u\te=%u\n", p->row, p->col, s, e);
#endif
	p->row = s/vs_cols;
	p->col = s%vs_cols;

	flags = p->struct_flags;
	clear_bit(NEED_WRAP, &p->struct_flags);
	clear_bit(DECIM, &p->struct_flags);
	set_bit(DECAWM, &p->struct_flags);
	SET_INPUT_STATE(p, RAW);
	if (inc_set)
		while (len--)
			if (vs_to_frame_(p, (p->row*vs_cols)+p->col) < 0) {
				s += vs_cols-frame_cols;
				len -= vs_cols-frame_cols-1;
				p->row = s/vs_cols;
				p->col = s%vs_cols;
			} else {
				write_data(p, p->fb[s++]);
				if (test_bit(NEED_WRAP, &p->struct_flags)) {
					cr(p);
					lf(p);
				}
			}
	else
		while (len--)
			if (vs_to_frame_(p, (p->row*vs_cols)+p->col) < 0) {
				s -= vs_cols-frame_cols;
				len -= vs_cols-frame_cols-1;
				p->row = s/vs_cols;
				p->col = s%vs_cols;
			} else {
				write_data(p, p->fb[s--]);
				if (test_bit(NEED_WRAP, &p->struct_flags)) {
					cr(p);
					lf(p);
				}
			}
	p->struct_flags = flags;

	p->row = row; p->col = col;
exit_func
}

static int show_cursor(struct lcd_struct *p)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int frame_base, frame_base_row, frame_base_col;
	unsigned int frame_rows = p->par->cntr_rows*p->par->num_cntr;
	unsigned int frame_cols = p->par->cntr_cols;
	unsigned int tmp = frame_cols/2;

	if (vs_rows == frame_rows && vs_cols == frame_cols)
		return (0);

enter_func
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
	/* cursor always on the lowest row of the display */
		frame_base_row = 0;
		frame_base_col = 0;
		if (p->row >= frame_rows)
			frame_base_row = p->row-(frame_rows-1);
		if (p->col >= frame_cols) {
			frame_base_col = p->col-(frame_cols-1);
			if (tmp) {
				tmp = (tmp-(frame_base_col%tmp))%tmp;
				if (frame_base_col+tmp <= vs_cols-frame_cols)
					frame_base_col += tmp;
			}
		}
	} else {
	/* cursor always on the uppermost row of the display */
		frame_base_row = vs_rows-frame_rows;
		frame_base_col = vs_cols-frame_cols;
		if (p->row < vs_rows-frame_rows)
			frame_base_row = p->row;
		if (p->col < vs_cols-frame_cols) {
			frame_base_col = p->col;
			if (tmp) {
				tmp = frame_base_col%tmp;
				if (frame_base_col >= tmp)
					frame_base_col -= tmp;
			}
		}
	}

	frame_base = p->frame_base;
	p->frame_base = (frame_base_row*vs_cols)+frame_base_col;
exit_func

	return (frame_base != p->frame_base);
}

/*
 * Move the visible screen area at user's wish
 */
static void browse_screen(struct lcd_struct *p, unsigned char dir)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int frame_base_row = p->frame_base/vs_cols;
	unsigned int frame_base_col = p->frame_base%vs_cols;
	unsigned int frame_rows = p->par->cntr_rows*p->par->num_cntr;
	unsigned int frame_cols = p->par->cntr_cols;

enter_func
	switch (dir) {
	case '1':	/* Up */
		if (! frame_base_row)
			return;
		--frame_base_row;
		break;
	case '2':	/* Down */
		if (frame_base_row >= vs_rows-frame_rows)
			return;
		++frame_base_row;
		break;
	case '3':	/* Left */
		if (! frame_base_col)
			return;
		--frame_base_col;
		break;
	case '4':	/* Right */
		if (frame_base_col >= vs_cols-frame_cols)
			return;
		++frame_base_col;
		break;
	default:
		return;
	}

	p->frame_base = (frame_base_row*vs_cols)+frame_base_col;
	redraw_screen(p, 0, p->fb_size-1);
exit_func
}

static inline void __memset_short(unsigned short *buf, unsigned short c, unsigned int len)
{
	while (len--)
		*buf++ = c;
}

/*
 * A memset implementation writing to LCD instead of memory locations.
 */
static void lcd_memset(struct lcd_struct *p, unsigned int d, unsigned short c, unsigned int len)
{
	unsigned int inc_set = test_bit(INC_CURS_POS, &p->struct_flags);

enter_func
	if (! len || d >= p->fb_size)
		return;

	if (inc_set && d+len > p->fb_size)
		len = p->fb_size-d;
	else if (! inc_set && len > d+1)
		len = d+1;

	if (! inc_set)
		d -= len-1;
	__memset_short(p->fb+d, c, len);

	if (show_cursor(p))
		redraw_screen(p, 0, p->fb_size-1);
	else
		redraw_screen(p, d, d+(len-1));
exit_func
}

static inline void __memcpy_short(unsigned short *d, unsigned short *s, unsigned int len, int dir)
{
enter_func
	if (dir > 0)
		while (len--)
			*d++ = *s++;
	else
		while (len--)
			*d-- = *s--;
exit_func
}

/*
 * A memmove implementation writing to LCD instead of memory locations.
 * Copy is done in a non destructive way. Display regions may overlap.
 */
static void lcd_memmove(struct lcd_struct *p, unsigned int d, unsigned int s, unsigned int len)
{
enter_func
	if (! len || d == s || d >= p->fb_size || s >= p->fb_size)
		return;

	if (d < s) {
		if (test_bit(INC_CURS_POS, &p->struct_flags)) {
			if (s+len > p->fb_size)
				len = p->fb_size-s;
		} else {
			if (len > d+1)
				len = d+1;
			d -= len-1;
			s -= len-1;
		}
		__memcpy_short(p->fb+d, p->fb+s, len, 1);
		if (show_cursor(p))
			redraw_screen(p, 0, p->fb_size-1);
		else
			redraw_screen(p, d, d+(len-1));
	} else {
		if (test_bit(INC_CURS_POS, &p->struct_flags)) {
			if (d+len > p->fb_size)
				len = p->fb_size-d;
			d += len-1;
			s += len-1;
		} else {
			if (len > s+1)
				len = s+1;
		}
		__memcpy_short(p->fb+d, p->fb+s, len, -1);
		if (show_cursor(p))
			redraw_screen(p, 0, p->fb_size-1);
		else
			redraw_screen(p, d-(len-1), d);
	}
exit_func
}

static void scrup(struct lcd_struct *p, unsigned int t, unsigned int b, unsigned int nr)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int d, s;

enter_func
	if (t+nr >= b)
		nr = b-t-1;
	if (b > vs_rows || t >= b || nr < 1)
		return;
	d = t*vs_cols;
	s = (t+nr)*vs_cols;
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		lcd_memmove(p, d, s, (b-t-nr)*vs_cols);
		lcd_memset(p, d+(b-t-nr)*vs_cols, p->erase_char, nr*vs_cols);
	} else {
		lcd_memmove(p, d+(b-t-nr)*vs_cols-1, s+(b-t-nr)*vs_cols-1, (b-t-nr)*vs_cols);
		lcd_memset(p, d+(b-t)*vs_cols-1, p->erase_char, nr*vs_cols);
	}
exit_func
}

static void scrdown(struct lcd_struct *p, unsigned int t, unsigned int b, unsigned int nr)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int d, s;

enter_func
	if (t+nr >= b)
		nr = b-t-1;
	if (b > vs_rows || t >= b || nr < 1)
		return;
	s = t*vs_cols;
	d = (t+nr)*vs_cols;
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		lcd_memmove(p, d, s, (b-t-nr)*vs_cols);
		lcd_memset(p, s, p->erase_char, nr*vs_cols);
	} else {
		lcd_memmove(p, d+(b-t-nr)*vs_cols-1, s+(b-t-nr)*vs_cols-1, (b-t-nr)*vs_cols);
		lcd_memset(p, s+nr*vs_cols-1, p->erase_char, nr*vs_cols);
	}
exit_func
}

static void lcd_insert_char(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int pos = (p->row*vs_cols)+p->col;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (test_bit(INC_CURS_POS, &p->struct_flags))
		lcd_memmove(p, pos+nr, pos, vs_cols-p->col-nr);
	else
		lcd_memmove(p, pos-nr, pos, p->col-(nr-1));
	lcd_memset(p, pos, p->erase_char, nr);
exit_func
}

static void lcd_delete_char(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int pos = (p->row*vs_cols)+p->col;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		lcd_memmove(p, pos, pos+nr, vs_cols-(p->col+nr));
		lcd_memset(p, (p->row+1)*vs_cols-nr, p->erase_char, nr);
	} else {
		lcd_memmove(p, pos, pos-nr, p->col-(nr-1));
		lcd_memset(p, (p->row*vs_cols)+(nr-1), p->erase_char, nr);
	}
exit_func
}





/******************************************************************************
 *************************      VT 102 Emulation      *************************
 ******************************************************************************/

/**********************
 * Control characters *
 **********************/
static void bs(struct lcd_struct *p)
{
enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		if (p->col)
			--p->col;
	} else {
		if (p->col+1 < p->par->vs_cols)
			++p->col;
	}
exit_func
}

static void cr(struct lcd_struct *p)
{
enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	p->col = (test_bit(INC_CURS_POS, &p->struct_flags) ? 0 : p->par->vs_cols-1);
exit_func
}

static void lf(struct lcd_struct *p)
{
enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		if (p->row+1 < p->bot)
			++p->row;
		else if (INPUT_STATE(p) != RAW) {
			show_cursor(p);
			scrup(p, p->top, p->bot, 1);
		}
	} else {
		if (p->row > p->top)
			--p->row;
		else if (INPUT_STATE(p) != RAW) {
			show_cursor(p);
			scrdown(p, p->top, p->bot, 1);
		}
	}
exit_func
}

static void ri(struct lcd_struct *p)
{
enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		if (p->row > p->top)
			--p->row;
		else {
			show_cursor(p);
			scrdown(p, p->top, p->bot, 1);
		}
	} else {
		if (p->row+1 < p->bot)
			++p->row;
		else {
			show_cursor(p);
			scrup(p, p->top, p->bot, 1);
		}
	}
exit_func
}

static void ff(struct lcd_struct *p)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (p->driver->clear_display) {
		p->driver->clear_display();
		__memset_short(p->fb, p->erase_char, p->fb_size);
		__memset_short(p->display, p->erase_char, p->frame_size);
		p->frame_base = 0;
	} else if (test_bit(INC_CURS_POS, &p->struct_flags))
		lcd_memset(p, 0, p->erase_char, p->fb_size);
	else
		lcd_memset(p, p->fb_size-1, p->erase_char, p->fb_size);

	if (test_bit(INC_CURS_POS, &p->struct_flags))
		p->row = p->col = 0;
	else {
		p->row = vs_rows-1;
		p->col = vs_cols-1;
	}
exit_func
}

static void tab(struct lcd_struct *p)
{
	struct lcd_parameters *par = p->par;
	unsigned int i, vs_cols = par->vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);

	if (! par->tabstop)
		return;

	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		i = par->tabstop-(p->col%par->tabstop);
		if (p->col+i < vs_cols)
			p->col += i;
	} else {
		i = p->col%par->tabstop;
		i = (i == 0 ? par->tabstop : i);
		if (p->col >= i)
			p->col -= i;
	}
exit_func
}

/*
 * Control character handler.
 */
static void control_char(struct lcd_struct *p, unsigned char val)
{
	switch (val) {
	case 0x08: 	/* BS: Back Space (^H) */
	case 0x7f:	/* DEL: Delete */
		bs(p);
		return;

	case 0x09: 	/* HT: Horizontal Tab (^I) */
		tab(p);
		return;

	case 0x0c: 	/* FF: Form Feed (^L) */
		ff(p);
		return;

	case 0x0a: 	/* LF: Line Feed (^J) */
	case 0x0b: 	/* VT: Vertical Tab (^K) */
		lf(p);
		if (! test_bit(CRLF, &p->struct_flags))
			return;

	case 0x0d: 	/* CR: Carriage Return (^M) */
		cr(p);
		return;

	case 0x16: 	/* SYN: Synchronous Idle (^V) */
		SET_INPUT_STATE(p, SYN);
		return;

	case 0x1b: 	/* ESC: Start of escape sequence */
		SET_INPUT_STATE(p, ESC);
		return;

	case 0x9b: 	/* CSI: Start of CSI escape sequence */
		memset(p->csi_args, 0, sizeof(p->csi_args));
		p->index = 0;
		SET_INPUT_STATE(p, CSI);
		return;
	}
}

static void gotoxy(struct lcd_struct *p, int new_col, int new_row)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;
	int min_row, max_row;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (test_bit(DECOM, &p->struct_flags)) {
		min_row = p->top;
		max_row = p->bot;
	} else {
		min_row = 0;
		max_row = vs_rows;
	}

	if (new_row < min_row)
		p->row = min_row;
	else if (new_row >= max_row)
		p->row = max_row-1;
	else
		p->row = new_row;

	if (new_col < 0)
		p->col = 0;
	else if (new_col >= vs_cols)
		p->col = vs_cols-1;
	else
		p->col = new_col;

	if (show_cursor(p))
		redraw_screen(p, 0, p->fb_size-1);
exit_func
}

static void gotoxay(struct lcd_struct *p, int new_col, int new_row)
{
enter_func
	gotoxy(p, new_col, test_bit(DECOM, &p->struct_flags) ? (p->top+new_row) : new_row);
exit_func
}


/******************************
 * ECMA-48 CSI ESC- sequences *
 ******************************/
static void csi_at(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	if (p->col+nr > vs_cols)
		nr = vs_cols-p->col;
	else if (! nr)
		++nr;
	lcd_insert_char(p, nr);
exit_func
}

static void csi_J(struct lcd_struct *p, unsigned int action)
{
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int pos = (p->row*vs_cols)+p->col;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
exit_func
	switch (action) {
	case 0:		/* From cursor to end of display */
		lcd_memset(p, pos, p->erase_char, p->fb_size-pos);
		return;

	case 1:		/* From start of display to cursor */
		lcd_memset(p, 0, p->erase_char, pos+1);
		return;

	case 2:		/* Whole display */
		lcd_memset(p, 0, p->erase_char, p->fb_size);
		return;
	}
}

static void csi_K(struct lcd_struct *p, unsigned int action)
{
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int row_start = p->row*vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
exit_func
	switch (action) {
	case 0:		/* From cursor to end of line */
		lcd_memset(p, row_start+p->col, p->erase_char, vs_cols-p->col);
		return;

	case 1:		/* From start of line to cursor */
		lcd_memset(p, row_start, p->erase_char, p->col+1);
		return;

	case 2:		/* Whole line */
		lcd_memset(p, row_start, p->erase_char, vs_cols);
		return;
	}
}

static void csi_L(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (p->row+nr > vs_rows)
		nr = vs_rows-p->row;
	else if (! nr)
		++nr;;
	lcd_memmove(p, (p->row+nr)*vs_cols, p->row*vs_cols, (vs_rows-p->row-nr)*vs_cols);
	lcd_memset(p, p->row*vs_cols, p->erase_char, nr*vs_cols);
exit_func
}

static void csi_M(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_rows = p->par->vs_rows;
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (p->row+nr > vs_rows)
		nr = vs_rows-p->row;
	else if (! nr)
		++nr;;
	lcd_memmove(p, p->row*vs_cols, (p->row+nr)*vs_cols, (vs_rows-p->row-nr)*vs_cols);
	lcd_memset(p, (vs_rows-nr)*vs_cols, p->erase_char, nr*vs_cols);
exit_func
}

static void csi_P(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	if (p->col+nr > vs_cols)
		nr = vs_cols-p->col;
	else if (! nr)
		++nr;
	lcd_delete_char(p, nr);
exit_func
}

static void csi_X(struct lcd_struct *p, unsigned int nr)
{
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
	if (p->col+nr > vs_cols)
		nr = vs_cols-p->col;
	else if (! nr)
		++nr;
	lcd_memset(p, (p->row*vs_cols)+p->col, p->erase_char, nr);
exit_func
}

static void csi_su(struct lcd_struct *p, unsigned char input)
{
	unsigned int vs_cols = p->par->vs_cols;

enter_func
	clear_bit(NEED_WRAP, &p->struct_flags);
exit_func
	if (input == 'u') {
		p->row = p->s_offset/vs_cols;
		p->col = p->s_offset%vs_cols;
		p->color = p->s_color;
		p->attributes = p->s_attributes;
		return;
	}
	p->s_offset = (p->row*vs_cols)+p->col;
	p->s_color = p->color;
	p->s_attributes = p->attributes;
}

static inline unsigned char reverse_color_attr(unsigned char attr)
{
	return ((attr & (BG_BRIGHT | FG_BRIGHT)) | ((attr & BG_COLOR) >> 4) | ((attr & FG_COLOR) << 4));
}

static unsigned char build_attr(struct lcd_struct *p, unsigned char color, unsigned char intensity,
				unsigned char blink, unsigned char underline, unsigned char reverse)
{
	unsigned char attr = color;

enter_func
	if (test_bit(CAN_DO_COLOR, &p->struct_flags)) {
		attr = color;
		if (underline)
			attr = (attr & BG_MASK) | p->ulcolor;
		else if (intensity == 0)
			attr = (attr & BG_MASK) | p->halfcolor;
	}
	if (reverse)
		attr = reverse_color_attr(attr);
	if (blink)
		attr ^= BG_BRIGHT;
	if (intensity == 2)
		attr ^= FG_BRIGHT;
	if (! test_bit(CAN_DO_COLOR, &p->struct_flags)) {
//		attr |= (underline ? ULINE : 0x00);
//		attr |= (reverse ? REVERSE : 0x00);
//		attr |= (blink ? BLINK : 0x00);
		if (underline)
			attr = (attr & 0xf8) | 0x01;
		else if (intensity == 0)
			attr = (attr & 0xf0) | 0x08;
	}
exit_func

	return (attr);
}

static void update_attr(struct lcd_struct *p)
{
	unsigned char intensity = p->attributes & I_MASK;
	unsigned char underline = (p->attributes & ULINE) ? 0x01 : 0x00;
	unsigned char reverse = (p->attributes & REVERSE) ? 0x01 : 0x00;
	unsigned char blink = (p->attributes & BLINK) ? 0x01 : 0x00;
	unsigned char decscnm = test_bit(DECSCNM, &p->struct_flags) ? 0x01 : 0x00;

enter_func
	p->attr = build_attr(p, p->color, intensity, blink, underline, reverse ^ decscnm);
	p->erase_char = (build_attr(p, p->color, 1, blink, 0, decscnm) << 8) | ' ';
exit_func
}

static void default_attr(struct lcd_struct *p)
{
enter_func
	p->attributes = 0x01;
	p->color = p->defcolor;
exit_func
}

static void lcd_invert_screen(struct lcd_struct *p, unsigned int s, unsigned int len)
{
	unsigned int l, inc_set = test_bit(INC_CURS_POS, &p->struct_flags);

enter_func
	if (! len || s >= p->fb_size)
		return;
	if (inc_set && s+len > p->fb_size)
		len = p->fb_size-s;
	else if (! inc_set && len > s+1)
		len = s+1;

	l = len;
	if (test_bit(CAN_DO_COLOR, &p->struct_flags))
		while (l--) {
			p->fb[s] = (reverse_color_attr(p->fb[s] >> 8) << 8) | (p->fb[s] & 0xff);
			++s;
		}
	else
		while (l--) {
			p->fb[s] ^= REVERSE << 8;
			++s;
		}

	if (show_cursor(p))
		redraw_screen(p, 0, p->fb_size-1);
	else
		redraw_screen(p, s, s+(len-1));
exit_func
}

unsigned char color_table[] = { 0, 4, 2, 6,
				1, 5, 3, 7,
				8,12,10,14,
				9,13,11,15 };

static void csi_m(struct lcd_struct *p, unsigned int n)
{
	int i, arg;

enter_func
	for (i = 0; i <= n; ++i)
		switch ((arg = p->csi_args[i]))
		{
			case 0:
				default_attr(p);
				break;

			case 1:
				p->attributes = (p->attributes & ~I_MASK) | 2;
				break;

			case 2:
				p->attributes = (p->attributes & ~I_MASK) | 0;
				break;

			case 4:
				p->attributes |= ULINE;
				break;

			case 5:
				p->attributes |= BLINK;
				break;

			case 7:
				p->attributes |= REVERSE;
				break;

			case 21: case 22:
				p->attributes = (p->attributes & ~I_MASK) | 1;
				break;

			case 24:
				p->attributes &= ~ULINE;
				break;

			case 25:
				p->attributes &= ~BLINK;
				break;

			case 27:
				p->attributes &= ~REVERSE;
				break;

			case 38:
				p->attributes |= ULINE;
				p->color = (p->color & BG_MASK) | (p->defcolor & FG_MASK);
				break;

			case 39:
				p->attributes &= ~ULINE;
				p->color = (p->color & BG_MASK) | (p->defcolor & FG_MASK);
				break;

			case 49:
				p->color = (p->defcolor & BG_MASK) | (p->color & FG_MASK);
				break;

			default:
				if (arg >= 30 && arg <= 37)
					p->color = (p->color & BG_MASK) | color_table[arg-30];
				else if (arg >= 40 && arg <= 47)
					p->color = (p->color & FG_MASK) | (color_table[arg-40] << 4);
				break;
		}

	update_attr(p);
exit_func
}

static void csi_h(struct lcd_struct *p, unsigned char n)
{
enter_func
exit_func
	switch (n) {
		case 4:		/* Set insert mode */
			set_bit(DECIM, &p->struct_flags);
			return;

		case 5:		/* Inverted screen mode */
			if (test_bit(QUES, &p->struct_flags) && ! test_bit(DECSCNM, &p->struct_flags)) {
				lcd_invert_screen(p, 0, p->fb_size);
				set_bit(DECSCNM, &p->struct_flags);
				update_attr(p);
			}
			return;

		case 6:		/* Cursor addressing origin: relative to scrolling region */
			if (test_bit(QUES, &p->struct_flags)) {
				set_bit(DECOM, &p->struct_flags);
				gotoxay(p, 0, 0);
			}
			return;

		case 7:		/* Set autowrap */
			if (test_bit(QUES, &p->struct_flags))
				set_bit(DECAWM, &p->struct_flags);
			return;

		case 20:	/* Set cr lf */
			set_bit(CRLF, &p->struct_flags);
			return;
	}
}

static void csi_l(struct lcd_struct *p, unsigned char n)
{
enter_func
exit_func
	switch (n) {
		case 4:		/* Reset insert mode */
			clear_bit(DECIM, &p->struct_flags);
			return;

		case 5:		/* Normal screen mode */
			if (test_bit(QUES, &p->struct_flags) && test_bit(DECSCNM, &p->struct_flags)) {
				lcd_invert_screen(p, 0, p->fb_size);
				clear_bit(DECSCNM, &p->struct_flags);
				update_attr(p);
			}
			return;

		case 6:		/* Cursor addressing origin: absolute origin */
			if (test_bit(QUES, &p->struct_flags)) {
				clear_bit(DECOM, &p->struct_flags);
				gotoxay(p, 0, 0);
			}
			return;

		case 7:		/* Reset autowrap */
			if (test_bit(QUES, &p->struct_flags))
				clear_bit(DECAWM, &p->struct_flags);
			return;

		case 20:	/* Reset cr lf */
			clear_bit(CRLF, &p->struct_flags);
			return;
	}
}

static void csi_linux(struct lcd_struct *p)
{
enter_func
exit_func
	switch (p->csi_args[0]) {
	case 1:
		if (test_bit(CAN_DO_COLOR, &p->struct_flags) && p->csi_args[1] < 16) {
			p->ulcolor = color_table[p->csi_args[1]];
			if (p->attributes & ULINE)
				update_attr(p);
		}
		return;

	case 2:
		if (test_bit(CAN_DO_COLOR, &p->struct_flags) && p->csi_args[1] < 16) {
			p->halfcolor = color_table[p->csi_args[1]];
			if ((p->attributes & I_MASK) == 0)
				update_attr(p);
		}
		return;

	case 8:
		p->defcolor = p->color;
		default_attr(p);
		update_attr(p);
		return;
	}
}

static void csi_r(struct lcd_struct *p, unsigned int top, unsigned int bot)
{
enter_func
	/* Minimum allowed region is 2 lines */
	if (top < bot) {
		p->top = top-1;
		p->bot = bot;
		gotoxay(p, 0, 0);
	}
exit_func
}

/*
 * ECMA-48 CSI ESC- sequence handler.
 */
static void handle_csi(struct lcd_struct *p, unsigned char input)
{
enter_func
exit_func
	if (p->index >= NPAR) {
		SET_INPUT_STATE(p, NORMAL);
		printf("LCD: too many parameters in CSI escape sequence\n");
	} else if (input == '?') {
		set_bit(QUES, &p->struct_flags);
	} else if (input == ';') {
		++p->index;
	} else if (input >= '0' && input <= '9') {
		p->csi_args[p->index] = (p->csi_args[p->index]*10)+(input-'0');
	} else {
		SET_INPUT_STATE(p, NORMAL);
		if (! test_bit(INC_CURS_POS, &p->struct_flags))
			return;
		switch (input) {
		case 'h':		/* DECSET sequences and mode switches */
			csi_h(p, p->csi_args[0]);
			clear_bit(QUES, &p->struct_flags);
			return;

		case 'l':		/* DECRST sequences and mode switches */
			csi_l(p, p->csi_args[0]);
			clear_bit(QUES, &p->struct_flags);
			return;
		}
		clear_bit(QUES, &p->struct_flags);
		switch (input) {
		case '@':		/* Insert # Blank character */
			csi_at(p, p->csi_args[0]);
			return;

		case 'G': case '`':	/* Cursor to indicated column in current row */
			if (p->csi_args[0])
				--p->csi_args[0];
			gotoxy(p, p->csi_args[0], p->row);
			return;

		case 'A':		/* Cursor # rows Up */
			if (! p->csi_args[0])
				++p->csi_args[0];
			gotoxy(p, p->col, p->row-p->csi_args[0]);
			return;

		case 'B': case 'e':	/* Cursor # rows Down */
			if (! p->csi_args[0])
				++p->csi_args[0];
			gotoxy(p, p->col, p->row+p->csi_args[0]);
			return;

		case 'C': case 'a':	/* Cursor # columns Right */
			if (! p->csi_args[0])
				++p->csi_args[0];
			gotoxy(p, p->col+p->csi_args[0], p->row);
			return;

		case 'D':		/* Cursor # columns Left */
			if (! p->csi_args[0])
				++p->csi_args[0];
			gotoxy(p, p->col-p->csi_args[0], p->row);
			return;

		case 'E':		/* Cursor # rows Down, column 1 */
			if (! p->csi_args[0])
				++p->csi_args[0];
			gotoxy(p, 0, p->row+p->csi_args[0]);
			return;

		case 'F':		/* Cursor # rows Up, column 1 */
			if (! p->csi_args[0])
				++p->csi_args[0];
			gotoxy(p, 0, p->row-p->csi_args[0]);
			return;

		case 'd':		/* Cursor to indicated row in current column */
			if (p->csi_args[0])
				--p->csi_args[0];
			gotoxay(p, p->col, p->csi_args[0]);
			return;

		case 'H': case 'f':	/* Cursor to indicated row, column (origin 1, 1) */
			if (p->csi_args[0])
				--p->csi_args[0];
			if (p->csi_args[1])
				--p->csi_args[1];
			gotoxay(p, p->csi_args[1], p->csi_args[0]);
			return;

		case 'J':		/* Erase display */
			csi_J(p, p->csi_args[0]);
			return;

		case 'K':		/* Erase line */
			csi_K(p, p->csi_args[0]);
			return;

		case 'L':		/* Insert # blank lines */
			csi_L(p, p->csi_args[0]);
			return;

		case 'M':		/* Delete # blank lines */
			csi_M(p, p->csi_args[0]);
			return;

		case 'P':		/* Delete # characters on the current line */
			csi_P(p, p->csi_args[0]);
			return;

		case 'X':		/* Erase # characters on the current line */
			csi_X(p, p->csi_args[0]);
			return;

		case 'm':		/* Set video attributes */
			csi_m(p, p->index);
			return;

		case 's':		/* Save cursor position */
		case 'u':		/* Restore cursor position */
			csi_su(p, input);
			return;

		case ']':		/* Linux private ESC [ ] sequence */
			csi_linux(p);
			return;

		case 'r':		/* Set the scrolling region */
			if (! p->csi_args[0])
				++p->csi_args[0];
			if (! p->csi_args[1] || p->csi_args[1] > p->par->vs_rows)
		 		p->csi_args[1] = p->par->vs_rows;
			csi_r(p, p->csi_args[0], p->csi_args[1]);
			return;

					/* Ignored escape sequences */
		case 'c':
		case 'g':
		case 'n':
		case 'q':
			return;

		default:
			printf("LCD: unrecognized CSI escape sequence: ESC [ %u\n", input);
			return;
		}
	}
}

/*
 * Custom ESC- sequence handler.
 */
static int handle_custom_esc(struct lcd_struct *p, unsigned int _input)
{
	unsigned char input = _input & 0xff;
	struct lcd_parameters *par = p->par;

enter_func
exit_func
	if (_input & (~0xff)) {
		switch (ESC_STATE(p)) {
		case 's':
			if (p->index++) {
				unsigned char *cgbuf = p->cgram_buffer-par->cgram_bytes;

				cgbuf[p->index-2] = input;
				if (p->index == par->cgram_bytes+1)
					write_cgram(p, p->cgram_index, cgbuf);
			} else {
				if (! p->driver->write_cgram_char) {
					printf("LCD: %s: missing function to write to CGRAM\n", p->par->name);
					return (-1);
				}
				if (input >= par->cgram_char0 && input < par->cgram_char0+par->cgram_chars)
					p->cgram_index = input;
				else {
					printf("LCD: bad CGRAM index\n");
					return (-1);
				}
			}
			return (0);

		case 'G':
			if (input >= par->cgram_char0 && input < par->cgram_char0+par->cgram_chars)
				write_data(p, (p->attr << 8) | p->driver->charmap[input]);
			else {
				SET_INPUT_STATE(p, NORMAL);
				handle_input(p, (p->attr << 8) | input);
			}
			return (0);

		case 'r':
			if (input == '1')
				address_mode(p, -1);
			else if (input == '0')
				address_mode(p, 1);
			return (0);

		case 'A':
			scrup(p, p->top, p->bot, input);
			return (0);

		case 'B':
			scrdown(p, p->top, p->bot, input);
			return (0);

		case 'C':
			browse_screen(p, input);
			return (0);
		}
	}

	/* These are the custom ESC- sequences */
	switch (input) {
	case 's':	/* CGRAM select */
		if (p->cgram_buffer) {
			SET_ESC_STATE(p, input);
			p->index = 0;
			return (par->cgram_bytes+1);
		} else {
			printf("LCD: driver %s does not support CGRAM chars\n", par->name);
			return (0);
		}

	case 'A':	/* Scroll up */
	case 'B':	/* Scroll down */
	case 'C':	/* Browse screen */
	case 'G':	/* Enter cgram mode */
	case 'r':	/* Decrement counter after data read/write */
		SET_ESC_STATE(p, input);
		return (1);
	}

	return (-1);
}

/*
 * ESC- but not CSI sequence handler.
 */
static int handle_esc(struct lcd_struct *p, unsigned char input)
{
	int ret;

enter_func
exit_func
	SET_INPUT_STATE(p, NORMAL);
	switch (input) {
	case 'c':	/* Reset */
		set_bit(DECAWM, &p->struct_flags);
		set_bit(INC_CURS_POS, &p->struct_flags);
		ff(p);
		return (0);

	case 'D':	/* Line Feed */
		lf(p);
		return (0);

	case 'E':	/* New Line */
		cr(p);
		lf(p);
		return (0);

	case 'M':	/* Reverse Line Feed */
		ri(p);
		return (0);

	case '7':
	case '8':
		csi_su(p, (input == '7' ? 's' : 'u'));
		return (0);

	/* CSI: Start of CSI escape sequence */
	case '[':
		memset(p->csi_args, 0, sizeof(p->csi_args));
		p->index = 0;
		SET_INPUT_STATE(p, CSI);
		return (0);

	/* Ignored escape sequences */
	case '(':
		SET_INPUT_STATE(p, ESC_G0);
		return (1);

	case ')':
		SET_INPUT_STATE(p, ESC_G1);
		return (1);

	case '#':
		SET_INPUT_STATE(p, ESC_HASH);
		return (1);

	case '%':
		SET_INPUT_STATE(p, ESC_PERCENT);
		return (1);

	case 'H':
	case 'Z':
	case '>':
	case '=':
	case ']':
		return (0);
	}

	/* These are the custom ESC- sequences */
	if ((ret = handle_custom_esc(p, input)) > 0) {
		SET_INPUT_STATE(p, ARG);
		return (ret);
	}

	if (ret < 0 && p->driver->handle_custom_char)
		if ((ret = p->driver->handle_custom_char(input)) > 0) {
			SET_INPUT_STATE(p, ARG_DRIVER);
			return (ret);
		}

	if (ret < 0)
		printf("LCD: unrecognized escape sequence: ESC %u\n", input);

	return (0);
}

/*
 * Main input handler.
 */
static void handle_input(struct lcd_struct *p, unsigned short _input)
{
	unsigned char input = _input & 0xff;
	struct lcd_driver *driver = p->driver;

	switch (INPUT_STATE(p)) {
	case NORMAL:
		if (input < 0x20 || input == 0x9b)
			control_char(p, input);
		else
			write_data(p, (_input & 0xff00) | driver->charmap[input]);
		return;

	case RAW:
		write_data(p, (_input & 0xff00) | driver->charmap[input]);
		return;

	case SYN:
		write_data(p, _input);
		SET_INPUT_STATE(p, NORMAL);
		return;

	case ESC:
		p->esc_args = handle_esc(p, input);
		return;

	case ESC_G0:
	case ESC_G1:
	case ESC_HASH:
	case ESC_PERCENT:
		if (! --p->esc_args)
			SET_INPUT_STATE(p, NORMAL);
		return;

	case CSI:
		handle_csi(p, input);
		return;

	case ARG:
		if (handle_custom_esc(p, 0x100 | input) || ! --p->esc_args)
			SET_INPUT_STATE(p, NORMAL);
		return;

	case ARG_DRIVER:
		if (driver->handle_custom_char(0x100 | input) || ! --p->esc_args)
			SET_INPUT_STATE(p, NORMAL);
		return;
	}
}





/***************************************
 * Read from/Write to display routines *
 ***************************************/

/*
 * Write character data to the display.
 */
static void write_data(struct lcd_struct *p, unsigned short data)
{
	unsigned int vs_cols = p->par->vs_cols;
	unsigned int pos;
	int frame_pos;

enter_func
	if (test_bit(NEED_WRAP, &p->struct_flags)) {
		cr(p);
		lf(p);
	}

	if (test_bit(DECIM, &p->struct_flags))
		lcd_insert_char(p, 1);

	pos = (p->row*vs_cols)+p->col;
	if ((frame_pos = vs_to_frame_(p, pos)) < 0) {
		show_cursor(p);
		redraw_screen(p, 0, p->fb_size-1);
		frame_pos = vs_to_frame_(p, pos);
	}

	if (p->display[frame_pos] != data) {
		p->driver->write_char(frame_pos, data);
		p->display[frame_pos] = data;
	}

#ifdef DEBUG
for (_i = 0; _i < _count-1; ++_i) printf("\t");
	printf(" writing char %c at (%d, %d)\n", data, p->row, p->col);
for (_i = 0; _i < _count-1; ++_i) printf("\t");
	printf(" attributes:\tblink:%d rev:%d ul:%d int:%d",
		((data >> 8) & BLINK ? 1 : 0),
		((data >> 8) & REVERSE ? 1 : 0),
		((data >> 8) & ULINE ? 1 : 0),
		(data >> 8) & I_MASK);
	if (test_bit(CAN_DO_COLOR, &p->struct_flags))
		printf(" bg_col:%d fg_col:%d",
			(data >> 12) & 0x0f,
			(data >> 8) & 0x0f);
	printf("\n");
#endif
	p->fb[pos] = data;

	if (test_bit(INC_CURS_POS, &p->struct_flags)) {
		if (p->col+1 < vs_cols)
			iterator_inc(p->col, vs_cols);
		else if (test_bit(DECAWM, &p->struct_flags))
			set_bit(NEED_WRAP, &p->struct_flags);
	} else {
		if (p->col)
			iterator_dec(p->col, vs_cols);
		else if (test_bit(DECAWM, &p->struct_flags))
			set_bit(NEED_WRAP, &p->struct_flags);
	}
exit_func
#ifdef DEBUG
if (!_count) printf("\n");
#endif
}

/*
 * Write an entire CGRAM character to the display.
 */
static void write_cgram(struct lcd_struct *p, unsigned char index, unsigned char *pixels)
{
	struct lcd_parameters *par = p->par;
	unsigned int inc_set = test_bit(INC_CURS_POS, &p->struct_flags);
	unsigned char *cgbuf = p->cgram_buffer+(index-par->cgram_char0)*par->cgram_bytes;

enter_func
	if (! strncmp((char *)cgbuf, (char *)pixels, par->cgram_bytes))
		return;

	if (! inc_set)
		address_mode(p, 1);

	p->driver->write_cgram_char(index, pixels);
	memcpy(cgbuf, pixels, par->cgram_bytes);

	if (! inc_set)
		address_mode(p, -1);
exit_func
}





/********************************
 * Init/Cleanup driver routines *
 ********************************/
static int do_init_driver(struct lcd_struct *p)
{
	int ret, init_level;
	struct lcd_driver *driver = p->driver;
	struct lcd_parameters *par = p->par;
	unsigned int frame_rows = par->cntr_rows*par->num_cntr;
	unsigned int frame_cols = par->cntr_cols;

	switch ((init_level = INIT_LEVEL(p))) {
	case 0:
		if (frame_rows == 0 || frame_cols == 0 || ! par->name) {
			printf("LCD: wrong lcd parameters\n");
			return (-EINVAL);
		}
		if (driver->validate_driver) {
			if ((ret = driver->validate_driver()) < 0) {
				printf("LCD: validate_driver failed\n");
				return (-EINVAL);
			} else if (ret > 0) {
				set_bit(CAN_DO_COLOR, &p->struct_flags);
				p->defcolor = 0x07;
				p->ulcolor = 0x03;
				p->halfcolor = 0x08;
			}
		}
		default_attr(p);
		update_attr(p);
		p->frame_size = frame_rows*frame_cols;
		if (par->vs_rows < frame_rows)
			par->vs_rows = frame_rows;
		if (par->vs_cols < frame_cols)
			par->vs_cols = frame_cols;
		p->fb_size = par->vs_rows*par->vs_cols;

		ret = sizeof(short)*p->fb_size;
		ret += sizeof(short)*p->frame_size;
		ret += FLIP_BUF_SIZE;
		ret += (p->driver->charmap ? 256 : 512);
		if (par->cgram_chars*par->cgram_bytes)
			ret += (1+par->cgram_chars)*par->cgram_bytes;
		if ((p->fb = (unsigned short *)malloc(ret)) == NULL) {
			printf("LCD: memory allocation failed (vmalloc)\n");
			return (-ENOMEM);
		}
		__memset_short(p->fb, p->erase_char, p->fb_size+p->frame_size);

		p->display = p->fb+p->fb_size;
		p->flip_buf = (unsigned char *)(p->display+p->frame_size);

		if (! p->driver->charmap) {
			set_bit(NULL_CHARMAP, &p->struct_flags);
			p->driver->charmap = p->flip_buf+FLIP_BUF_SIZE;
			for (ret = 0; ret < 256; ++ret)
				p->driver->charmap[ret] = ret;
			p->s_charmap = p->driver->charmap+256;
		} else
			p->s_charmap = p->flip_buf+FLIP_BUF_SIZE;
		memset(p->s_charmap, 0, 256);

		if (par->cgram_chars*par->cgram_bytes) {
			p->cgram_buffer = p->s_charmap+256+par->cgram_bytes;
			memset(p->cgram_buffer, 0, par->cgram_chars*par->cgram_bytes);
		} else
			p->cgram_buffer = NULL;

		p->frame_base = 0;
		p->row = p->col = 0;
		p->top = 0;
		p->bot = par->vs_rows;
		SET_INIT_LEVEL(p, ++init_level);

	case 1:
		/* Initialize the communication port */
		if ((ret = driver->init_port())) {
			printf("LCD: failure while initializing the communication port\n");
			return (ret);
		}
		SET_INIT_LEVEL(p, ++init_level);

	case 2:
		/* Initialize LCD display */
		if (driver->init_display && (ret = driver->init_display())) {
			printf("LCD: failure while initializing the display\n");
			return (ret);
		}
		SET_INIT_LEVEL(p, ++init_level);
	}

	return (0);
}

static int do_cleanup_driver(struct lcd_struct *p)
{
	int ret, init_level;
	struct lcd_driver *driver = p->driver;

	switch ((init_level = INIT_LEVEL(p))) {
	case 3:
		if (driver->cleanup_display && (ret = driver->cleanup_display())) {
			printf("LCD: failure while cleaning the display\n");
			return (ret);
		}
		SET_INIT_LEVEL(p, --init_level);

	case 2:
		if ((ret = driver->cleanup_port())) {
			printf("LCD: failure while cleaning the communication port\n");
			return (ret);
		}
		SET_INIT_LEVEL(p, --init_level);

	case 1:
		if (test_bit(NULL_CHARMAP, &p->struct_flags)) {
			p->driver->charmap = NULL;
			clear_bit(NULL_CHARMAP, &p->struct_flags);
		}
		free(p->fb);
		p->fb = NULL;
		SET_INIT_LEVEL(p, --init_level);
	}

	return (0);
}

static int init_driver(struct lcd_struct *p)
{
	int ret;

	if ((ret = do_init_driver(p))) {
		do_cleanup_driver(p);
		printf("LCD: init_driver failed\n");
	}

	return (ret);
}

static int cleanup_driver(struct lcd_struct *p)
{
	int ret;

	if ((ret = do_cleanup_driver(p))) {
		do_init_driver(p);
		printf("LCD: cleanup_driver failed\n");
	}

	return (ret);
}

/**************************************************
 * Kernel register/unregister lcd driver routines *
 **************************************************/
/* Exported function */
int lcd_register_driver(struct lcd_driver *driver, struct lcd_parameters *par)
{
	int ret;
	struct lcd_struct *p;

	if (! driver->write_char || ! driver->init_port || ! driver->cleanup_port) {
		printf("LCD: missing functions\n");
		return (-EINVAL);
	}
	if ((p = (struct lcd_struct *)malloc(sizeof(struct lcd_struct))) == NULL) {
		printf("LCD: memory allocation failed (kmalloc)\n");
		return (-ENOMEM);
	}
	memset(p, 0, sizeof(struct lcd_struct));
	p->driver = driver;
	p->par = par;
	SET_INIT_LEVEL(p, 0);
	SET_INPUT_STATE(p, NORMAL);
	clear_bit(CRLF, &p->struct_flags);
	set_bit(DECAWM, &p->struct_flags);
	set_bit(INC_CURS_POS, &p->struct_flags);

	if ((ret = init_driver(p))) {
		free(p);
		return (ret);
	}

	lcd_drivers = p;

	return (0);
}

/* Exported function */
int lcd_unregister_driver(struct lcd_driver *driver, struct lcd_parameters *par)
{
	int ret;
	struct lcd_struct *p;

	p = lcd_drivers;

	if ((ret = cleanup_driver(p)))
		return (ret);
	free(p);

	return (0);
}

struct lcd_parameters par = {
	.name = "prova",

	.tabstop = 8,
	.num_cntr = 1,

	.cntr_rows = 5,
	.cntr_cols = 4,

	.vs_rows = 10,
	.vs_cols = 20,

	.cgram_chars = 8,
	.cgram_bytes = 8,
};

void write_char(unsigned int offset, unsigned short data)
{
}

int validate_driver(void)
{
	return (2);
}

int init_port(void)
{
	return (0);
}

int cleanup_port(void)
{
	return (0);
}


struct lcd_driver driver = {
	.write_char		= write_char,
	.validate_driver	= validate_driver,
	.init_port		= init_port,
	.cleanup_port		= cleanup_port,
};

void print_buf(unsigned short *buf, unsigned int rows, unsigned int cols, unsigned int start, unsigned int size, struct lcd_struct *p)
{
	unsigned int i;

	printf("    ");
	for (i = 2; i <= cols; i += 2)
		printf(" %d", i%10);
	printf("\n");

	printf("   +");
	for (i = 0; i < cols; ++i)
		printf("-");
	printf("+\n");

	for (i = 0; i < size; ++i) {
		unsigned short c = buf[start++];

		if (! (i%cols))
			printf("%2d |", 1+((i/cols)%rows));
		if (test_bit(CAN_DO_COLOR, &p->struct_flags)) {
			int arg;

			arg = (c >> 8) & 0x0f;
			if (arg >= 0 && arg <= 7)
				printf("\033[%dm", color_table[arg]+30);
			arg = (c >> 12) & 0x0f;
			if (arg >= 0 && arg <= 7)
				printf("\033[%dm", color_table[arg]+40);
		} else {
			if ((c & 0x0300) == 0x0100)
				printf("\033[21m");
			if ((c & 0x0300) == 0x0200)
				printf("\033[1m");
			if ((c & 0x0300) == 0x0000)
				printf("\033[2m");
			if (c & 0x0400)
				printf("\033[4m");
			if (c & 0x8000)
				printf("\033[5m");
			if (c & 0x0800)
				printf("\033[7m");
		}
		printf("%c\033[0m", (c < 0x20 ? '?' : c));
		if (! ((i+1)%cols))
			printf("|\n");
	}

	printf("   +");
	for (i = 0; i < cols; ++i)
		printf("-");
	printf("+\n");
}

typedef unsigned long loff_t;

static inline loff_t offset_to_row_col(struct lcd_struct *p, loff_t offset)
{
	unsigned long _offset = offset;
	unsigned int vs_cols = p->par->vs_cols;

	gotoxy(p, _offset%vs_cols, _offset/vs_cols);

	return ((p->row*vs_cols)+p->col);
}

int main(int argc, char **argv)
{
	struct lcd_struct *p;
	char c;

	if ((errno = -lcd_register_driver(&driver, &par))) {
		perror("register failed");
		exit(1);
	}
	p = lcd_drivers;

	while ((c = getchar()) != EOF)
		handle_input(p, (p->attr << 8) | c);

	print_buf(p->fb, p->par->vs_rows, p->par->vs_cols, 0, p->fb_size, p);
	print_buf(p->display, p->par->cntr_rows*p->par->num_cntr, p->par->cntr_cols, 0, p->frame_size, p);
//	print_buf(p->cgram_buffer, p->par->cgram_chars, p->par->cgram_bytes, p->par->cgram_chars*p->par->cgram_bytes, p);
//	printf("fb_base = %u\tvs_base = %u\tvs_end = %u\tfb_end = %u\n",
//		p->fb_base, p->vs_base, vs_end(p), fb_end(p));
	printf("row = %u\tcol = %u\n", p->row, p->col);

	if ((errno = -lcd_unregister_driver(&driver, &par))) {
		perror("unregister failed");
		exit(1);
	}

	exit(0);
}
