/* test_ioctl.c
 *
 * $Id: test_ioctl.c,v 1.9 2010/07/17 19:08:24 mjona Exp $
 *
 * How to use ioctl on the device.
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define LCD_LINUX_MAIN
#include <linux/hd44780.h>

int main(int argc, char **argv)
{
	int fd;
	struct lcd_parameters param;

	if ((fd = open("/dev/lcd", O_RDONLY)) == -1) {
		perror("/dev/lcd");
		exit(EXIT_FAILURE);
	}

	if (ioctl(fd, LCDL_GET_PARAM, &param) == -1) {
		perror("ioctl");
		exit(EXIT_FAILURE);
	}

	if (atoi(argv[1]) & 1)
		param.flags |= HD44780_CHECK_BF;
	else
		param.flags &= ~HD44780_CHECK_BF;

	if (atoi(argv[1]) & 2)
		param.flags |= HD44780_4BITS_BUS;
	else
		param.flags &= ~HD44780_4BITS_BUS;

	if (ioctl(fd, LCDL_SET_PARAM, &param) == -1) {
		perror("ioctl");
		exit(EXIT_FAILURE);
	}

	if (close(fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
