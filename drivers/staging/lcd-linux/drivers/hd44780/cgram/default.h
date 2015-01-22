/* default.h
 *
 * $Id: default.h,v 1.2 2010/07/17 19:08:24 mjona Exp $
 *
 * Default user defined characters for lcdmod.
 *
 * Copyright (C) by Michael McLellan (mikey@cs.auckland.ac.nz)
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
 *
 */

static void init_charmap(void)
{
}

static unsigned char cg0[] = { 0x1f, 0x1f, 0x11, 0x0f, 0x11, 0x1e, 0x01, 0x1f };
static unsigned char cg1[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f };
static unsigned char cg2[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f };
static unsigned char cg3[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f };
static unsigned char cg4[] = { 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f };
static unsigned char cg5[] = { 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f };
static unsigned char cg6[] = { 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f };
static unsigned char cg7[] = { 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f };
