/*******************************************************************************
  Copyright(c) 2012-2013-2014 Joaquin Bogado.
  Code based on the QHY5 Linux driver from
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  Code Based on the QHY5TDLL Windows Driver From
  Copyright(c) 2007-2009, Tom Van den Eede
  Copyright(c) 2007 QHYCCD
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <usb.h>
#include <pthread.h>

#define QHY_EAST  0x10
#define QHY_NORTH 0x20
#define QHY_SOUTH 0x40
#define QHY_WEST  0x80
#define MAX( a, b ) ( ( a > b) ? a : b )

/******************************************************
******* Device handling (locate, open, close)**********/
typedef struct {
	usb_dev_handle *handle;
	uint16_t width;
	uint16_t height;
	uint16_t offw;
	uint16_t offh;
	uint8_t binmode;
	uint16_t gg1;
	uint16_t gb;
	uint16_t gr;
	uint16_t gg2;
	uint16_t vblank;
	uint16_t hblank;
	uint8_t bpp;
	uint16_t etime;
	void *image;
	size_t framesize;
}qhy5t_driver;
/** Device handling (locate, open, close)**********/

qhy5t_driver *qhy5t_open();

void qhy5t_set_params(qhy5t_driver *qhy5t, uint16_t w, uint16_t h, uint16_t x, uint16_t y, uint8_t bin, 
					uint16_t gg1, uint16_t gb, uint16_t gr, uint16_t gg2, uint16_t vblank, uint16_t hblank,
					uint8_t bpp, uint16_t etime);

int qhy5t_program_camera(qhy5t_driver *qhy5t, int reprogram);

void qhy5t_start_capture(qhy5t_driver * qhy5t);

void * qhy5t_read_exposure(qhy5t_driver *qhy5t);

int qhy5t_stop_capture(qhy5t_driver * qhy5t);

void qhy5t_close(qhy5t_driver *qhy5t);

int qhy5_timed_move(qhy5t_driver *qhy5t, int direction, int duration_msec);

int qhy5t_cancel_move(qhy5t_driver * qhy5t);

int qhy5t_set_gain(int gain);

int qhy5t_timed_move(qhy5t_driver *qhy5t, int direction, int duration_msec);

/** END Device handling (locate, open, close)******/

void write_pgm(void * data, int width, int height, char *filename);


