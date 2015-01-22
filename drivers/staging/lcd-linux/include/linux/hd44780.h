/* hd44780.h
 *
 * $Id: hd44780.h,v 1.18 2010/07/17 19:08:24 mjona Exp $
 *
 * LCD-Linux:
 * Driver for HD44780 compatible displays connected to the parallel port.
 *
 * HD44780 header file.
 *
 * Copyright (C) 2004 - 2010  Mattia Jona-Lasinio (mjona@users.sourceforge.net)
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

#ifndef HD44780_H
#define HD44780_H

#include <linux/lcd-linux.h>

#define HD44780_VERSION		LCD_LINUX_VERSION	/* Version number */
#define HD44780_STRING		"hd44780"

#define HD44780_MINOR		0	/* Minor number for the hd44780 driver */


/* flags */
#define HD44780_CHECK_BF	0x00000001	/* Do busy flag checking */
#define HD44780_4BITS_BUS	0x00000002	/* Set the bus length to 4 bits */
#define HD44780_5X10_FONT	0x00000004	/* Use 5x10 dots fonts */

/* IOCTLs */
#define HD44780_READ_AC		_IOR(LCD_MAJOR, 0x00, unsigned char *)

#endif /* HD44780 included */
