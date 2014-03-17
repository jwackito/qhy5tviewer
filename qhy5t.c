/*******************************************************************************
  Copyright(c) 2012-2013-2014 Joaquin Bogado joaquinbogado at gmail.com 
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
//To build standalone app:
//gcc -DQHY5T_TEST -Wall -o qhy5t qhy5t.c -lusb -lpthread

#include "qhy5t.h"

#define dprintf if(debug > 0) printf
#define ddprintf if(debug > 1) printf

#ifdef DEBUG
int debug=1;
#else
int debug=0;
#endif

#define READ 0xC0
#define WRITE 0x40
#define BULKINEP 0x82
#define BULKOUTEP 0x01
#define ERR_BINMODE -3
#define ERR_OFFSET -4
#define ERR_READIMAGE -5

#define HHSB(x) ((uint8_t)(((uint32_t)x>>24)&0xFF))
#define HSB(x) ((uint8_t)(((uint32_t)x>>16)&0xFF))
#define MSB(x) ((uint8_t)(((uint32_t)x>>8)&0xFF))
#define LSB(x) ((uint8_t)(((uint32_t)x)&0xFF))

#define BUFBYTE(W) buffer[bi++] = LSB(W)
#define BUFWORD(W) buffer[bi++] = MSB(W); buffer[bi++] = LSB(W)
#define BUFTRIP(W) buffer[bi++] = LSB(W); buffer[bi++] = MSB(W);  buffer[bi++] = HSB(W)
#define BUFWORDR(W) *((uint16_t *)&buffer[bi]) =W ; bi+=2;
#define BUFDWORD(W) buffer[bi++] = HHSB(W); buffer[bi++] = HSB(W); buffer[bi++] = MSB(W) ;buffer[bi++] = LSB(W)

pthread_mutex_t reading, writing; //mutexes for exposure
pthread_t expo_thread;

static void * buffer;

int ctrl_msg(usb_dev_handle *handle, unsigned char request_type, unsigned char request, unsigned int value, unsigned int index, uint8_t *data, unsigned char len);
void qhy5t_reconnect(qhy5t_driver * qhy5t);
/******************************************************
******* Device handling (locate, open, close)**********/
//private
usb_dev_handle *qhy5t_locate(unsigned int vid, unsigned int pid){
	unsigned char located = 0;
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle *device_handle = NULL;
	
	usb_find_busses();
	usb_find_devices();
	for (bus = usb_busses; bus; bus = bus->next)
	{
		for (dev = bus->devices; dev; dev = dev->next){
			if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid){
				located++;
				device_handle = usb_open(dev);
				{//ddprintf("open = 0x%x\n", device_handle);
				ddprintf("Filename: %s \n", dev->filename);
				ddprintf("Dirname: %s \n", dev->bus->dirname);
				ddprintf("nIfaces: %d \n", dev->config->bNumInterfaces);
				ddprintf("bConf: %d \n", dev->config->bConfigurationValue);
				ddprintf("iConf: %d \n", dev->config->iConfiguration);
				ddprintf("bIntNumber: %d \n", dev->config->interface->altsetting->bInterfaceNumber);
				ddprintf("iInterf: %d \n", dev->config->interface->altsetting->iInterface);
				ddprintf("Vendor ID 0x0%x\n",dev->descriptor.idVendor);
				ddprintf("Product ID 0x0%x\n",dev->descriptor.idProduct);
				}
			}
		}	
	}
	if (device_handle==NULL){
		return NULL;
	}
	usb_set_configuration(device_handle, 1);
	usb_claim_interface(device_handle, 0);
	usb_set_altinterface(device_handle, 0);
	return device_handle;
}
qhy5t_driver *qhy5t_open(){
	struct usb_dev_handle *handle;
	qhy5t_driver *qhy5t;
	usb_init();
	if ((handle = qhy5t_locate(0x1618, 0x0910))==NULL){
		printf("Could not open the QHY5T device\n");
		return NULL;
	}
	qhy5t = calloc(sizeof(qhy5t_driver), 1);
	qhy5t->handle = handle;
	return qhy5t;
}

int qhy5t_stop_capture(qhy5t_driver * qhy5t){
	int error=0;
	usb_bulk_write(qhy5t->handle, BULKOUTEP, "0", 1, 5000);
	return error;
}

void qhy5t_close(qhy5t_driver *qhy5t){
	if(! qhy5t)
		return;
	qhy5t_reconnect(qhy5t);
	/** freed by read_exposure()
	 if(qhy5t->image)
		free(qhy5t->image);*/
	if(qhy5t->handle)
		usb_close(qhy5t->handle);
	free(qhy5t);
	return;
}
/** END Device handling (locate, open, close)**********
*******************************************************/

/******************************************************
* Device programming, control and exposure ************/

int ctrl_msg(usb_dev_handle *handle, unsigned char request_type, unsigned char request, 
				unsigned int value, unsigned int index, uint8_t *data, unsigned char len){
	int i, result;
	static int countmsg=0;
	dprintf("\n");
	dprintf("Sending %s command 0x%02x, 0x%02x, 0x%04x, 0x%04x, %d bytes\n",
		(request_type & USB_ENDPOINT_IN) ? "recv" : "send",
		request_type, request, value, index, len);
	result = usb_control_msg(handle, request_type,
				request, value, index, (char*)data, len, 5000);
	for(i = 0; i < len; i++) {
		dprintf(" %02x", (unsigned char)data[i]);
	}
	dprintf("\n");
	dprintf("msj# %d: Bytes writen = %d\n", countmsg, result);
	countmsg++;
	return result;
}
void qhy5t_set_params(qhy5t_driver *qhy5t, uint16_t w, uint16_t h, uint16_t x, uint16_t y, uint8_t bin, 
					uint16_t gg1, uint16_t gb, uint16_t gr, uint16_t gg2, uint16_t vblank, uint16_t hblank, uint8_t bpp, uint16_t etime){

	w = qhy5t->width = (w >= 4 && w <=2048) ? w : 2048;
	h = qhy5t->height = (h >= 4 && h <=1536) ? h : 1536;
	qhy5t->offw =  (x >= 0 && x <=(2048 - w)) ? x : (2048 - w) / 2;
	qhy5t->offh =  (y >= 0 && y <=(1536 - h)) ? y : (1536 - h) / 2;
	qhy5t->binmode = 1;
	qhy5t->gg1 = (gg1 > 0 && gg1 < 0x7860) ? gg1 : 1;
	qhy5t->gb = (gb > 0 && gb < 0x7860) ? gb : 1;
	qhy5t->gr = (gr > 0 && gr < 0x7860) ? gr : 1;
	qhy5t->gg2 = (gg2 > 0 && gg2 < 0x7860) ? gg2 : 1;
	qhy5t->vblank = vblank;
	qhy5t->hblank = hblank;
	qhy5t->bpp = 8;
	if (etime > 1)
		qhy5t->etime = etime;
	if (etime > 60000)
		qhy5t->etime = 60000;
	qhy5t->image = calloc(2,w*h);
	qhy5t->framesize=0;
}
int qhy5t_program_camera(qhy5t_driver *qhy5t, int reprogram){
	static uint8_t buffer[64];
	static uint8_t keep[64];
	int bi = 0;
	
	//temp variables
	uint32_t w,h,x,y;
	
	uint32_t ETIME,
			GG1, GB, GR, GG2,
			X, Y,
			H, W,
			HBLANK,
			VBLANK,
			REG22,
			REG23;
	uint16_t SHUTTER;
			//GPIF,
	int		PRE,
			FRAME,
			POST,
			RESET;
	ETIME = (uint32_t)qhy5t->etime;
	GG1 = (uint32_t)qhy5t->gg1;
	GR = (uint32_t)qhy5t->gr;
	GB = (uint32_t)qhy5t->gb;
	GG2 = (uint32_t)qhy5t->gg2;
	if (qhy5t->binmode < 1 || qhy5t->binmode > 2){return ERR_BINMODE;}
	
	if(reprogram){
		goto reprogram;
	}
	else{
		if (qhy5t->etime > 60000){
			qhy5t->etime = 60000;
		}
		W = w = (uint32_t)qhy5t->width;
		H = h = (uint32_t)qhy5t->height;
		X = x = (uint32_t)qhy5t->offw;
		Y = y = (uint32_t)qhy5t->offh;
		if (W + x > 2048||H + y > 1536){return ERR_OFFSET;}
		int _VB = qhy5t->vblank-25;
		VBLANK = (uint32_t)(qhy5t->vblank);
		HBLANK = (uint32_t)qhy5t->hblank;
		dprintf("H %X\n",qhy5t->etime);
		switch (qhy5t->binmode){
			case 1: //binning 1x1
				REG22 = 0x00;
				REG23 = 0x00;
				W--;
				H--;
				X &= 0xFFFE;
				W &= 0xFFFE;
				H &= 0xFFFE;
				W++;
				H++;
				X += 0x20;	//Skip overscan
				Y += 0x14;	//Skip overscan
				uint32_t preblank = (331 + 38);
				//uint16_t fullwidth = (2050 + HBLANK + preblank);
				uint32_t totalwidth = (W + 3)+HBLANK+preblank;
				uint32_t totalheight = (H + 1)+VBLANK+1;
				uint32_t framesize = totalwidth * totalheight;
				
				RESET = (1538 - (_VB * 2) - (H + 1)) * totalwidth +
						(2048 - (W + 1)) * (totalheight - _VB + 10) +
						(2048 - (W + 1)) * (1536 - (H + 1));
				
				if ((framesize / 24000) < ETIME) {            //See is Rolling shutter applies
					SHUTTER = (uint16_t)(totalheight-1);
					ETIME -= (framesize/24000);
					dprintf("Appling rolling shutter: shtr = %d, etime = %d\n", SHUTTER, ETIME);
				}
				else {
					SHUTTER = MAX(1,((uint16_t)((ETIME * 24000)/(totalwidth))));
					RESET += (SHUTTER+1)* (totalwidth);
					ETIME=0;
					dprintf("Not appling rolling shutter: shtr = %d, etime = %d, reset = 0x%06X\n", SHUTTER, ETIME, RESET);
				}
				RESET %= framesize; 
				RESET += 3997158 - preblank -316 - 12 + framesize;
				PRE =  (VBLANK) * totalwidth;
				FRAME = totalwidth * h;
				if (FRAME%0x200)
					FRAME += 0x200 - (FRAME%0x200);
				POST  = framesize - PRE - FRAME;
				qhy5t->framesize = FRAME;
				break;
		}
	}
	//Almost quoted from Tom's driver.
	BUFWORDR((uint16_t)(ETIME));
	////// REGISTERS IN CAMERA
	BUFWORD(GG1);  
	BUFWORD(GB);
	BUFWORD(GR);
	BUFWORD(GG2);
	BUFWORD((Y));
	BUFWORD((X));
	BUFWORD((H));
	BUFWORD((W));
	BUFWORD((HBLANK));
	BUFWORD((VBLANK));
	BUFWORD(REG22);
	BUFWORD(REG23);
	BUFDWORD(SHUTTER);             
	BUFWORD(3);   //REGISTER 0x62
	/////// GPIF TRANSFER SIZES FOR FRAME/PRE/POST
	BUFBYTE(0x5F);
	BUFWORD(0x00);
	BUFTRIP((PRE));                //PRE_READ 
	BUFTRIP((FRAME));              //data size
	BUFTRIP((POST));               //POST READ
	BUFTRIP((RESET));               //RESET READ
	
	dprintf("%06X\n",(PRE));                //PRE_READ 
	dprintf("%06X\n",(FRAME));              //data size
	dprintf("%06X\n",(POST));               //POST READ
	dprintf("%06X\n",(RESET));
	
	//We only write the info if something change
reprogram:	if (memcmp(buffer, keep,64)) {
		qhy5t_reconnect(qhy5t); //reconnect before program the camera registers
		ctrl_msg(qhy5t->handle, WRITE, 0x11, 0, 0, buffer, 64);
		uint32_t t = (uint32_t)ETIME+1000;
		ctrl_msg(qhy5t->handle, READ, 0x55, 0x0000, 0x0063, (uint8_t*)&t, 0x0004);
		usleep(5000);
		memcpy(keep , buffer, 64 );
	}
	return 0;
	
}

void * qhy5t_exposure_thread(void * ptr){
	qhy5t_driver * qhy5t = (qhy5t_driver *)ptr;
	int result;
	int timeout = qhy5t->etime + 5000;
	uint32_t preblank = (331+38); //ONLY WORKS FOR 1x1 BIN
	uint32_t totalwidth = (qhy5t->width + 2)+(qhy5t->hblank)+preblank;
	int i;
	uint8_t *localbuffer = NULL;
	localbuffer = calloc(2,qhy5t->framesize);
	while (1){
		result = usb_bulk_read(qhy5t->handle, BULKINEP, localbuffer, qhy5t->framesize, timeout);
		dprintf("Locked writing in ET\n");
		pthread_mutex_lock(&writing);
		if (result != qhy5t->framesize) {
			printf("Failed to read image. Got: %08X, expected %08X\n", result, (unsigned int)qhy5t->framesize);
			free(buffer);
			buffer = NULL;
		}
		else{//mapping the frame
			char *pb=localbuffer;
			void * pi = buffer;
			for (i=0; i< qhy5t->height; i++){
				memcpy(pi, pb, qhy5t->width);
				pi+=qhy5t->width;
				pb+=totalwidth;
			}
		}
		dprintf("Unocked reading in ET\n");
		pthread_mutex_unlock(&reading);
		if (buffer == NULL) break;
	}
	printf("Cancelling exposure\n");
	pthread_exit(NULL);
	return NULL;
}

void qhy5t_reconnect(qhy5t_driver * qhy5t){
	printf("Reconnecting\n");
	ctrl_msg(qhy5t->handle, WRITE, 0xA0, 0xE600, 0, (uint8_t *)"1", 1);
	usleep(3000);
	ctrl_msg(qhy5t->handle, WRITE, 0xA0, 0xE600, 0, (uint8_t *)"0", 1);
	usleep(3000);
}

void qhy5t_start_capture(qhy5t_driver * qhy5t){
	dprintf("Creating buffers\n");
	buffer = calloc(2 * qhy5t->width * qhy5t->height,1);
	dprintf("Locked reading in SE\n");
	pthread_mutex_lock(&reading);
	dprintf("Starting exposure\n");
	pthread_create( &expo_thread, NULL, qhy5t_exposure_thread, (void*) qhy5t);
}

int qhy5t_set_gain(int gain){
//from data in datasheet MT9T001
	if (gain <= 0){
		return 0x0008;
	}
	if (gain <= 24){ // gain 1 - 4, step 0.125
		return 0x0008 + gain;
	}
	if (gain <= 24 + 15){ //gain 4.25 - 8, step 0.25
		return 0x0051 + (gain - 24);
	}
	if (gain <= 24 + 15 + 128){ // gain 9 - 128, step 1
		return 0x60 + ((gain - 24 - 15) << 7);
	}
	if (gain > 24 + 15 + 128){
		return 0x7860;
	}
}


/** END Device programming, control and exposure ******
*******************************************************/

/******************************************************
* Mount Guiding commands ************************/

void guide_command(qhy5t_driver * qhy5t, uint16_t cmd, uint16_t pulsetimex, uint16_t pulsetimey){
	uint16_t duration[2] = {0, 0};
	if (pulsetimex != 0xFFFFFFFF) pulsetimex *=10;
	if (pulsetimey != 0xFFFFFFFF) pulsetimey *=10;

	duration[0] = pulsetimex;
	duration[1] = pulsetimey;
	
	int status = ctrl_msg(qhy5t->handle, WRITE, 0x10, 0, cmd, (char *)&duration, sizeof(duration));
	printf("Status = %d\ncommand = %d\n", status, cmd);
}

int qhy5t_timed_move(qhy5t_driver *qhy5t, int direction, int duration_msec){
	
	uint16_t duration[2] = {0, 0};
	uint16_t cmd;

	if (! (direction & (QHY_NORTH | QHY_SOUTH | QHY_EAST | QHY_WEST))) {
		fprintf(stderr, "No direction specified to qhy5t_timed_move\n");
		return 1;
	}

	if (duration_msec > 0){
		switch (direction){
		case QHY_NORTH:
		case QHY_SOUTH:
			duration[1] = duration_msec;
			break;
		case QHY_WEST:
		case QHY_EAST:
			duration[0] = duration_msec;
			break;
		}
	}
	cmd &= direction;
	
	return ctrl_msg(qhy5t->handle, WRITE, 0x10, 0, cmd, (char *)&duration, sizeof(duration));
}

int qhy5t_cancel_move(qhy5t_driver * qhy5t){
	///cancel quiding
	///From Geoffrey Hausheer for QHY5
	uint16_t ret;
	int cmd = 0x18;
	/**if ((direction & (QHY_NORTH | QHY_SOUTH)) &&
		(direction & (QHY_EAST | QHY_WEST)))
	{
		cmd = 0x18;
	} else if(direction & (QHY_NORTH | QHY_SOUTH)) {
		cmd = 0x21;
	} else {
		cmd = 0x22;
	}*/
	printf("Cancel guiding command requested\n");
	return ctrl_msg(qhy5t->handle, READ, 0, 0, cmd, (char *)&ret, sizeof(ret));
}


/** END Mount Guiding commands ******
*******************************************************/

/******************************************************
* Image manipution and mapping ************************/

void * qhy5t_read_exposure(qhy5t_driver *qhy5t){
	dprintf("Locked reading in RE\n");
	pthread_mutex_lock(&reading);
	if (buffer == NULL){
		free(qhy5t->image);
		return qhy5t->image = NULL;
	}
	memcpy(qhy5t->image, buffer, (qhy5t->width*qhy5t->height));
	dprintf("Unlocked writing in RE\n");
	pthread_mutex_unlock(&writing);
	return qhy5t->image;
}

void write_pgm(void * data, int width, int height, char *filename){
	FILE *h = fopen(filename, "w");
	dprintf("writing file %s w x h =  %dx%d\n", filename, width, height);
	fprintf(h, "P5\n"); //pgm header
	fprintf(h, "%d %d\n", width, height);//pgm header
	fprintf(h, "255\n");//pgm header
	fwrite(data, width*height, 1, h);
	fclose(h);
}

/** END Image manipulation and mapping ****************
*******************************************************/

#ifdef QHY5T_TEST
#include <getopt.h>
void show_help(char * progname){
	printf("%s [options]\n", progname);
	printf("\t\t-x/--width <width>                specify width (default: 2048)\n");
	printf("\t\t-y/--height <height>              specify height (default: 1536)\n");
	printf("\t\t-g/--gain <gain>                  specify gain [0-167] (default 1)\n");
	printf("\t\t-b/--binning <bin>                specify the binning mode (2x2 or default: 1x1)\n");
	printf("\t\t-t/--exposure <exposure>          specify exposure in msec (default: 100)\n");
	printf("\t\t-f/--file <filename>              specify filename to write to\n");
	printf("\t\t-c/--count <count>                specify how many sequential images to take. If -c isn't especified,\n");
	printf("\t\t                                  then the output file will be exactly <filename> and will be a fits file. \n"); 
	printf("\t\t                                  (This is for QHYImager compatibility). Else, will be <filename>0000x.<fmt>\n");
	printf("\t\t-m/--format <fmt>                 specify the file type (default: ppm, else fits file will be created.)\n");
	printf("\t\t-d/--debug                        enable debugging\n");
	printf("\t\t-h//-help                         show this message\n");
	exit(0);
}

int main (int argc,char **argv){
	qhy5t_driver *qhy5t;
	int i;
	int width = 2048;
	int height = 1536;
	int offw = (2048 - width) / 2;
	int offh = (1536 - height) / 2;
	int count = 1;
	int bin = 1; //binmode default 1x1
	int bpp = 1;
	int hblank = 142;
	unsigned int vblank = 25;
	unsigned int gain = 1;
	unsigned int etime = 100;
	char fmt[10] = "ppm";
	char basename[256] = "image";
	char imagename[256];
	int error;
	void * data = NULL;
	
	
	struct option long_options[] = {
		{"exposure" ,required_argument, NULL, 't'},
		{"gain", required_argument, NULL, 'g'},
		{"binning", required_argument, NULL, 'b'},
		{"vblank", required_argument, NULL, 'k'},
		{"width", required_argument, NULL, 'x'},
		{"height", required_argument, NULL, 'y'},
		{"debug", required_argument, NULL, 'd'},
		{"file", required_argument, NULL, 'f'},
		{"count", required_argument, NULL, 'c'},
		{"format", required_argument, NULL, 'm'},
		{"help", no_argument , NULL, 'h'},
		{0, 0, 0, 0}
	};

	while (1) {
		char c;
		c = getopt_long (argc, argv, 
                     "t:g:b:k:x:y:df:c:m:h",
                     long_options, NULL);
		if(c == EOF)
			break;
		switch(c) {
		case 't':
			etime = strtol(optarg, NULL, 0);
			break;
		case 'g':
			gain = strtol(optarg, NULL, 0);
			//Gain calculations for QHY5T from the datasheet
			gain = qhy5t_set_gain(gain);
			break;
		case 'b':
			bin = atoi(optarg);
			break;
		case 'k':
			vblank = atoi(optarg);
			break;
		case 'x':
			width = strtol(optarg, NULL, 0);
			offw = (2048 - width) / 2;
			break;
		case 'y':
			height = strtol(optarg, NULL, 0);
			offh = (1536 - height) / 2;
			break;
		case 'f':
			strncpy(basename, optarg, 255);
			break;
		case 'd':
			debug = 1;
			break;
		case 'c':
			count = strtol(optarg, NULL, 0);
			if ((count%2)==0) count--;
			break;
		case 'm':
			strncpy(fmt, optarg, 10);
			break;
		case 'h':
			show_help(argv[0]);
			break;
		}
	}
	
	if(width > 2048 || width < 1) {
		printf("Width must be between 1 and 1280\n");
		exit(1);
	}
	
	if(height > 1536 || height < 1) {
		printf("Height must be between 1 and 1536\n");
		exit(1);
	}
	
	if((qhy5t = qhy5t_open())== NULL) {
		printf("Could not open the QHY5T device\n");
		exit(-1);
	}

	printf("Capturing %dx%d\n", width, height);
	printf("Exposing for %f sec at gain: %d\n", etime / 1000.0, gain);

	error = qhy5t_set_params(qhy5t, width, height, offw, offh, bin, gain, gain, gain, gain,
					vblank, hblank, bpp, etime);
	if (error){printf("Error %d\n", error); exit(error);}
	
	qhy5t_reconnect(qhy5t);
	error = qhy5t_programcamera(qhy5t, 0);
	qhy5t_start_exposure(qhy5t);
	
	i=0;
	do{
		sprintf(imagename, "%s%05d.%s", basename, i, fmt);
		printf("Capturing %s\n", imagename);
		data = qhy5t_read_exposure(qhy5t);
		write_pgm(data, qhy5t->width, qhy5t->height, imagename);
	}while(i++ != count);
	qhy5t_stopcapture(qhy5t);
	qhy5t_close(qhy5t);
	return 0;
}

#endif
