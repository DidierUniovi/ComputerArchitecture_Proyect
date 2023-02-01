/*
 * Main.cpp
 *
 *  Created on: Fall 2019
 */

#include <stdio.h>
#include <math.h>
#include <CImg.h>
#include <time.h>

using namespace cimg_library;

// Data type for image components
typedef float data_t;

// Declaration of the constants
const char* SOURCE_IMG = "bailarina.bmp";	//Constatans for the images
const char* FILTER_IMG = "background_V.bmp";
const char* DESTINATION_IMG = "bailarina2.bmp";
const uint REPEAT_ALGORITHM = 60;	//NUmber of times the algorith is going to be repeated

// Filter argument data type (Includes source Image and Destination)
typedef struct {
	data_t *pRsrc; // Pointers to the R, G and B components
	data_t *pGsrc;
	data_t *pBsrc;
	data_t *pRdst;
	data_t *pGdst;
	data_t *pBdst;
	uint pixelCount; // Size of the image in pixels
} filter_args_t;

// Filter Image argument data type
typedef struct{
	data_t *pRfilter;
	data_t *pGfilter;
	data_t *pBfilter;
	uint filterPixelCount; // Size of the image in pixels
} filter_Image;

// Removes the possible color saturation from the pixels
void colorSaturation(filter_args_t args0, int i){
	if(*(args0.pRdst+i) >255){
		*(args0.pRdst+i)=255;
	}
	else if (*(args0.pRdst+i)<0){
		*(args0.pRdst+i)=0;
	}
	if(*(args0.pGdst+1)>255){
		*(args0.pGdst+i)=255;
	}
	else if (*(args0.pGdst+i)<0){
		*(args0.pGdst+i)=0;
	}
	if(*(args0.pBdst+i) >255){
		*(args0.pBdst+i)=255;
	}
	else if (*(args0.pBdst+i)<0){
		*(args0.pBdst+i)=0;
	}
}

/***********************************************
 *
 * Algorithm. Image filter.
 * Blend: Overlap Mode #10
 *
 * *********************************************/

// Our algorithm (Overlap) does not output saturated colors
// given that the input images are also in the [0, 255] range,
// however the definition below can be uncommented in order to
// check the output. (note that the performance can be affected).

//#define CHECK_COLOR_SATURATION

void filter (filter_args_t args0, filter_Image args1) {

    for (uint i = 0; i < args0.pixelCount; i++) {
		*(args0.pRdst + i) = (*(args1.pRfilter + i) / 255 ) * ( *(args1.pRfilter + i) + ((2 * *(args0.pRsrc + i)) / 255) * (255 - *(args1.pRfilter + i)));
		*(args0.pGdst + i) = (*(args1.pGfilter + i) / 255 ) * ( *(args1.pGfilter + i) + ((2 * *(args0.pGsrc + i)) / 255) * (255 - *(args1.pGfilter + i)));
		*(args0.pBdst + i) = (*(args1.pBfilter + i) / 255 ) * ( *(args1.pBfilter + i) + ((2 * *(args0.pBsrc + i)) / 255) * (255 - *(args1.pBfilter + i)));
		#ifdef CHECK_COLOR_SATURATION
		colorSaturation(args0, i);
		#endif
	}
}

int main() {

	cimg::exception_mode(0);

	// Open file and object initialization
	CImg<data_t> srcImage;
	try{

		CImg<data_t> loadImage(SOURCE_IMG);
		srcImage = loadImage;
	} catch( CImgIOException& e ){
		printf("Failed to open the source image. Expected name: %s\n", SOURCE_IMG);
		exit(EXIT_FAILURE);
	}

	filter_args_t filter_args;
	data_t *pDstImage; // Pointer to the new image pixels

	// Filter Image initialization
	CImg<data_t> filterImage;

	try{
		CImg<data_t> loadImage(FILTER_IMG);
		filterImage = loadImage;
	} catch( CImgIOException& e ){
		printf("Failed to open the filter image. Expected name: %s\n", FILTER_IMG);
		exit(EXIT_FAILURE);
	}

	filter_Image filter_components;
	uint widthFilter = filterImage.width();// Getting information from the Filter image
	uint heightFilter = filterImage.height();

	// Time variables
	struct timespec tStart, tEnd;
    double dElapsedTime;

	srcImage.display(); // Displays the source image
	uint width = srcImage.width();// Getting information from the source image
	uint height = srcImage.height();
	uint nComp = srcImage.spectrum();// source image number of components

	// Common values for spectrum (number of image components):
	//  B&W images = 1
	//	Normal color images = 3 (RGB)
	//  Special color images = 4 (RGB and alpha/transparency channel)

	// Calculating image size in pixels
	filter_args.pixelCount = width * height;
	filter_components.filterPixelCount = widthFilter * heightFilter;

	// Checking that Image and Filter have the same size.
	if(height != heightFilter || width != widthFilter){
		perror("Source Image and Filter Image don't have the same pixel size!!");
		exit(EXIT_FAILURE);
	}

	// Allocate memory space for destination image components
	pDstImage = (data_t *) malloc (filter_args.pixelCount * nComp * sizeof(data_t));
	if (pDstImage == NULL) {
		perror("Allocating destination image");
		exit(-2);
	}

	// Pointers to the componet arrays of the source image
	filter_args.pRsrc = srcImage.data(); // pRcomp points to the R component array
	filter_args.pGsrc = filter_args.pRsrc + filter_args.pixelCount; // pGcomp points to the G component array
	filter_args.pBsrc = filter_args.pGsrc + filter_args.pixelCount; // pBcomp points to B component array

	// Pointers to the component arrays of the Filter image
	filter_components.pRfilter = filterImage.data(); // pRcomp points to the R component array
	filter_components.pGfilter = filter_components.pRfilter + filter_components.filterPixelCount; // pGcomp points to the G component array
	filter_components.pBfilter = filter_components.pGfilter + filter_components.filterPixelCount; // pBcomp points to B component array

	// Pointers to the RGB arrays of the destination image
	filter_args.pRdst = pDstImage;
	filter_args.pGdst = filter_args.pRdst + filter_args.pixelCount;
	filter_args.pBdst = filter_args.pGdst + filter_args.pixelCount;


	// Measuring start time
	if(clock_gettime(CLOCK_REALTIME, &tStart) == -1){
		perror("Clock_gettime Error!!");
		exit(EXIT_FAILURE);
	}

	// ALGORITHM
	for(uint i = 0; i < REPEAT_ALGORITHM; i++){
		filter(filter_args, filter_components);
	}

	// Measuring end time
	if(clock_gettime(CLOCK_REALTIME, &tEnd) == -1){
		perror("Clock_gettime Error!!");
		exit(EXIT_FAILURE);
	}

	// Calculating elapsed time
	dElapsedTime = (tEnd.tv_sec - tStart.tv_sec);
	dElapsedTime += (tEnd.tv_nsec - tStart.tv_nsec) / 1e+9;

	printf("\n");
	printf("Elapsed time: %.4f", dElapsedTime);
	printf("\n");

	// Create a new image object with the calculated pixels
	// In case of normal color images use nComp=3,
	// In case of B/W images use nComp=1.
	CImg<data_t> dstImage(pDstImage, width, height, 1, nComp);

	// Store destination image in disk
	dstImage.save(DESTINATION_IMG);

	// Display destination image
	dstImage.display();

	// Free memory
	free(pDstImage);

	return 0;
}
