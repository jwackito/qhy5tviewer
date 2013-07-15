/*
 * ppmdebayer.c
 * 
 * Copyright 2013 Joaquin Bogado <jwackito@themule>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int
//#define dprintf if(debug > 0) printf
#define debug 0

#define max(a,b) (a)>(b)?(a):(b)
#define min(a,b) (a)<(b)?(a):(b)

/*Demosaicing using smooth hue transition interpolation*/
//pix position relative in source image [rgrg...gbgb]
#define SRCTL(ptr) (*(ptr-w-1))
#define SRCTR(ptr) (*(ptr-w+1))
#define SRCBL(ptr) (*(ptr+w-1))
#define SRCBR(ptr) (*(ptr+w+1))
#define SRCL(ptr) (*(ptr-1))
#define SRCR(ptr) (*(ptr+1))
#define SRCT(ptr) (*(ptr-w))
#define SRCB(ptr) (*(ptr+w))
//pix position relative in target [rgbrgbrgbrgb] color: 0=r, 1=g, 2=blue
#define TGTTL(ptr,color) (*(ptr-w*3-3+color))
#define TGTTR(ptr,color) (*(ptr-w*3+3+color))
#define TGTBL(ptr,color) (*(ptr+w*3-3+color))
#define TGTBR(ptr,color) (*(ptr+w*3+3+color))
#define TGTL(ptr,color) (*(ptr-3+color))
#define TGTR(ptr,color) (*(ptr+3+color))
#define TGTT(ptr,color) (*(ptr-w*3+color))
#define TGTB(ptr,color) (*(ptr+w*3+color))

#define TGR(ptr) (*(ptr))
#define TGG(ptr) (*(ptr+1))
#define TGB(ptr) (*(ptr+2))

#define max(a,b) (a)>(b)?(a):(b)
#define min(a,b) (a)<(b)?(a):(b)

enum color {red, green1, green2, blue};
//enum color {green2, blue, red, green1};
//enum color {red, green1, green2, blue};
//enum color {blue, green2, green1, red};

void * debayer_data_jwack(void * data, void * dest, int width, int height){
	uint16_t i, j, h, w;
	uint8_t * tgt;
	uint8_t * src;
	
	unsigned int tr,tg,tb;
	w = width;
	h = height;
	tgt = (uint8_t *)dest;
	src = (uint8_t *)data;
	
	tgt = memset(tgt, 1,w*h*3);
	//lineal four neighbour interpolation 
	*tgt = 255;
	src += w;
	tgt += w*3;
	for (j=1; j < h; j++){
		for (i=1; i < w; i++){
			if (i%2 == 1 && j%2 ==0){// red pixel
				tr = *src;
				tb = 0.25*(SRCTL(src) + SRCTR(src) + SRCBL(src) + SRCBR(src));
				tg = 0.25*(SRCT(src) + SRCL(src) + SRCB(src) + SRCR(src));
			}
			if (i%2 == 0 && j%2 ==0){//green1 pixel
				tg = *src;
				tb = 0.50*(SRCT(src) + SRCB(src));
				tr = 0.50*(SRCL(src) + SRCR(src));
			}
			if (i%2 == 1 && j%2 ==1){//green2 pixel
				tg = *src;
				tr = 0.50*(SRCT(src) + SRCB(src));
				tb = 0.50*(SRCL(src) + SRCR(src));
			}
			if (i%2 == 0 && j%2 ==1){// blue pixel
				tb = *src;
				tr = 0.25*(SRCTL(src) + SRCTR(src) + SRCBL(src) + SRCBR(src));
				tg = 0.25*(SRCT(src) + SRCL(src) + SRCB(src) + SRCR(src));
			}
			src++;
			*tgt++ = (uint8_t)tr;
			*tgt++ = (uint8_t)tg;
			*tgt++ = (uint8_t)tb;
		}
		src++;
		tgt+=3;
	}
	return dest;
}

void *debayer_data_shti(void * data, void * dest, int width, int height){
	uint16_t i, j, h, w;
	uint8_t * tgt;
	uint8_t * src;
	
	unsigned int tr,tg,tb;
	w = width;
	h = height;
	tgt = (uint8_t *)dest;
	src = (uint8_t *)data;
	tgt = memset(tgt, 1,w*h*3);
	//green interpolation
	//first row
	src++;
	tgt+=3;
	for (i=1; i < w/2+1; i++){
		TGR(tgt) = (uint8_t)*src;
		TGG(tgt) = (uint8_t)max(0.3*((SRCL(src) + SRCR(src) + SRCB(src))),1);
		src++;
		tgt+=3;
		TGG(tgt) = (uint8_t)max(*src,1);
		src++;
		tgt+=3;
	}
	src++;
	tgt+=3;
	
	for (j=1; j < h-1; j++){
		for (i=1; i < w-1; i++){
			if (i%2 == 1 && j%2 ==0){// red pixel
				TGG(tgt) = (uint8_t)(max(0.25*(SRCT(src) + SRCL(src) + SRCB(src) + SRCR(src)),1));
				TGR(tgt) = (uint8_t)*src;
			}
			if (i%2 == 0 && j%2 ==0){//green1 pixel
				TGG(tgt) = (uint8_t)(max(*src,1));
			}
			if (i%2 == 1 && j%2 ==1){//green2 pixel
				TGG(tgt) = (uint8_t)(max(*src,1));
			}
			if (i%2 == 0 && j%2 ==1){// blue pixel
				TGG(tgt) = (uint8_t)(max(0.25*(SRCT(src) + SRCL(src) + SRCB(src) + SRCR(src)),1));
				TGB(tgt) = (uint8_t)*src;
			}
			src++;
			tgt+=3;
		}
		src+=2;
		tgt+=6;
	}
	//rest of the frame
	tgt = (uint8_t *)dest;
	src = (uint8_t *)data;
	src+=w+1;
	tgt+=w*3+3;
	for (j=1; j < h-1; j++){
		for (i=1; i < w-1; i++){
			if (i%2 == 1 && j%2 == 0){// red pixel
				TGR(tgt) = (uint8_t)(0.25 * TGG(tgt) * (SRCTL(src) +
														SRCTR(src) +
														SRCBL(src) +
														SRCBR(src)));
				TGB(tgt) = (uint8_t)*src;
			}
			if (i%2 == 0 && j%2 == 0){//green1 pixel
				TGB(tgt) = (uint8_t)(0.50 * TGG(tgt) * (SRCL(src)  + 
														SRCR(src)));
				TGR(tgt) = (uint8_t)(0.50 * TGG(tgt) * (SRCT(src)  + 
														SRCB(src) ));
			}
			if (i%2 == 1 && j%2 == 1){//green2 pixel
				TGR(tgt) = (uint8_t)(0.50 * TGG(tgt) * (SRCL(src)  + 
														SRCR(src) ));
				TGB(tgt) = (uint8_t)(0.50 * TGG(tgt) * (SRCT(src)  + 
														SRCB(src) ));
			}
			if (i%2 == 0 && j%2 == 1){// blue pixel
				TGB(tgt) = (uint8_t)(0.25 * TGG(tgt) * (SRCTL(src) +
														SRCTR(src) +
														SRCBL(src) +
														SRCBR(src) ));
				TGR(tgt) = (uint8_t)*src;
			}
			src++;
			tgt+=3;
		}
		src+=2;
		tgt+=6;
	}
	return dest;
}

void * debayer(void * buffer, int w, int h, int bpp){
	void * tmp;
	tmp = malloc(w*h*3*bpp);
	tmp = debayer_data_jwack(buffer, tmp, w,h-1);
	free(buffer);
	return tmp;
}

void * read_ppm5(char * filename, int * width, int * height, int * bpp){
	FILE * fp;
	void * buffer;
	char magick[3];
	fp = fopen(filename, "r");
	if (fp == NULL){
		return NULL;
	}
	fgets(magick, 3, fp);
	if (strcmp(magick, "P5")){
		printf("Not a P5 ppm file\n");
		return NULL;
	}
	fgetc(fp);
	fscanf(fp, "%d %d", width, height);
	fscanf(fp, "%d", bpp);
	if ((*bpp) < 256){(*bpp) = 1;} else{ (*bpp) = 2;}
	*bpp=1;
	buffer = malloc((*width) * (*height) * (*bpp));
	fread(buffer, (*width) * (*height) * (*bpp), 1, fp);
	return buffer;
}

void write_ppm6(void * data, int width, int height, char *basename){
	FILE *fp;
	char * filename = malloc(strlen(basename) + 15);
	sprintf(filename, "%s%s","_dby_", basename );
	fp = fopen(filename, "w");
	if (fp == NULL) {printf("%s\n", filename);return;}
	fprintf(fp, "P6\n"); 
	fprintf(fp, "%d %d\n", width, height);
	fprintf(fp, "255\n");
	fwrite(data, width*height*3, 1, fp);
	free(data);
	fclose(fp);
}

void write_ppm(void * data, int width, int height, char *filename){
	FILE *h = fopen(filename, "w");
	fprintf(h, "P5\n"); //ppm header
	fprintf(h, "%d %d\n", width, height);//ppm header
	fprintf(h, "255\n");//ppm header
	fwrite(data, width*height, 1, h);
	fclose(h);
}

/**
 * The program receive a list of P5 ppm file names and for each one,
 * outpust a new file P6 debayyerized file (no overwrite the original)
 **/
int main(int argc, char **argv)
{
	void * buffer;
	int w,h, bpp; /*width, height, and bits per pixel*/
	char * filename;
	int images = 1;
	while (images != argc){
		filename = argv[images];
		buffer =  read_ppm5(filename, &w, &h, &bpp);
		if (buffer == NULL){
			printf("Can't process file %s\n", filename);
			images++;
		}
		buffer = debayer(buffer, w, h, bpp);
		write_ppm6(buffer, w, h, filename);
		images++;
	}
	return 0;
}

