/* test_read.c
 *
 * $Id: test_read.c,v 1.10 2010/07/17 19:08:24 mjona Exp $
 *
 * LCD driver for HD44780 compatible displays connected to the parallel port.
 *
 * How to read from the device.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int fd, i, bytes, rbytes;
	unsigned char buffer[256];

	if (argc-3) {
		fprintf(stderr, "Usage: %s <offset> <length>\n"
			"\tRead <length> bytes from /dev/lcd\n"
			"\tstarting at <offset>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	bytes = atoi(argv[2]);

	if ((fd = open("/dev/lcd", O_RDONLY)) == -1) {
		perror("/dev/lcd");
		exit(EXIT_FAILURE);
	}

	if (lseek(fd, atoi(argv[1]), SEEK_SET) == (off_t)-1) {
		perror("lseek");
		exit(EXIT_FAILURE);
	}

	if ((rbytes = read(fd, buffer, bytes)) == (ssize_t)-1) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < rbytes; ++i)
		printf("%1$u\t'%1$c'\n", buffer[i]);

	if (close(fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
