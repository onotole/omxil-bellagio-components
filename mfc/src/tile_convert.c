/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 * mfc_func.c: MFC related functions
 */

/*
 * This code was written with the purpose of having a simple and easy to understand
 * function to change the NV12MT pixel format to YUV420. It hasn't been optimized
 * for performance.
 *
 * If performance is on your mind then you should definitely use FIMC as the color
 * converter.
 */

static inline int nv12_tile(int x, int y, int w, int h)
{
        int rx, ry, dx, dy;
        /* The Z and reverse Z ordering */
        /* For more information please look at the following web site:
         * http://linuxtv.org/downloads/v4l-dvb-apis/re27.html
         */
        static int t[8] = {0, 1, 6, 7, 2, 3, 4, 5};

        rx = x % 4;
        ry = y % 2;
        dx = x / 4;
        dy = y / 2;

        if (((h % 2) == 1) && (y == (h - 1)))
                return dy * w * 2 + x;
        else
                return t[rx + ry * 4] + dy * w * 2 + dx * 8;
}

void Y_tile_to_linear_4x2(unsigned char *p_linear_addr, unsigned char *p_tiled_addr, unsigned int x_size, unsigned int y_size)
{
	int x, y;

	int x_tiles, y_tiles;
	int d_x, d_y;
	int r_x, r_y;

	x_tiles = (x_size + 127) / 128;
	x_tiles <<= 1;
	y_tiles = (y_size + 31) / 32;

	for (y = 0; y < y_size; y++)
	for (x = 0; x < x_size; x++) {
		r_x = x % 64;
		r_y = y % 32;
		d_x = x / 64;
		d_y = y / 32;

		p_linear_addr[x + y * x_size] = p_tiled_addr[ r_x   + r_y * 64 + nv12_tile(d_x, d_y, x_tiles, y_tiles) * 64 * 32];
	}
}

void CbCr_tile_to_linear_4x2(unsigned char *p_linear_addr, unsigned char *p_tiled_addr, unsigned int x_size, unsigned int y_size)
{
	int x, y;

	int x_tiles, y_tiles;
	int d_x, d_y;
	int r_x, r_y;
	int offset;
	int nv_offset;

	offset = x_size * y_size / 4;

	y_size /= 2;

	x_tiles = (x_size + 127) / 128;
	x_tiles <<= 1;
	y_tiles = (y_size + 31) / 32;

	for (y = 0; y < y_size; y++)
	for (x = 0; x < x_size; x+=2) {
		r_x = x % 64;
		r_y = y % 32;
		d_x = x / 64;
		d_y = y / 32;
		nv_offset = nv12_tile(d_x, d_y, x_tiles, y_tiles) * 64 * 32;

		p_linear_addr[x/2 + y * x_size/2] = p_tiled_addr[ r_x   + r_y * 64 + nv_offset];
		p_linear_addr[x/2 + y * x_size/2 + offset] = p_tiled_addr[ r_x   + r_y * 64 + nv_offset + 1];
	}
}
