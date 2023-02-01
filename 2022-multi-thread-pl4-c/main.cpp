/*
 * Main.cpp
 *
 *  Created on: Fall 2022
 */

#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <CImg.h>

using namespace cimg_library;

// Data type for image components
typedef float data_t;

// Name of images files
const char *SOURCE_IMG = "bailarina.bmp";
const char *FILTER_IMG = "background_V.bmp";
const char *DESTINATION_IMG = "bailarina2.bmp";

// Number of times the Blend Algorithm is repeated
const uint REPEAT_ALGORITHM = 60;

// Number of thread to use: 4 processors with 4 cores each - No Hyperthreading: 1 thread per core
const uint NUMBER_OF_THREADS = 16;

// Filter argument data type (Includes source Image and Destination)
typedef struct{
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
	uint filterPixelCount; // Size of the filter in pixels
} filter_image;

typedef struct {
	filter_args_t imageSrc;
	filter_image filterImage;
	uint pixelInit;
	uint pixelEnd;
	uint id;
} thread_args;

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

// Our algorithm (Overlap) does not output saturated colors
// given that the input images are also in the [0, 255] range,
// however the definition below can be uncommented in order to
// check the output. (note that the performance can be affected).

//#define CHECK_COLOR_SATURATION

void* FilterThread(void* args){

	thread_args params = *((thread_args *)args);

	data_t XR, XG, XB, YR, YG, YB;
    filter_args_t args0 = params.imageSrc;
    filter_image args1 = params.filterImage;

	for (uint i = params.pixelInit; i < params.pixelEnd; i++){

		// Source images data
		XR = *(args0.pRsrc + i);
		XG = *(args0.pGsrc + i);
		XB = *(args0.pBsrc + i);
		YR = *(args1.pRfilter + i);
		YG = *(args1.pGfilter + i);
		YB = *(args1.pBfilter + i);

		// Destination image
		*(args0.pRdst + i) = (YR / 255) * (YR + ((2 * XR) / 255) * (255 - YR));
		*(args0.pGdst + i) = (YG / 255) * (YG + ((2 * XG) / 255) * (255 - YG));
		*(args0.pBdst + i) = (YB / 255) * (YB + ((2 * XB) / 255) * (255 - YB));
		#ifdef CHECK_COLOR_SATURATION
		colorSaturation(args0, i);
		#endif
	}

	return NULL;
}

void filterProcess(filter_args_t filter_args, filter_image filter_components){

	// Array with all the threads and its corresponding params
	pthread_t threads[NUMBER_OF_THREADS];
	thread_args params[NUMBER_OF_THREADS];

	// Create threads
	for (uint i = 0; i < NUMBER_OF_THREADS; i++){

		// Set the data for each thread. Each thread will process a specific part of the array
		params[i].id = i;
		params[i].imageSrc = filter_args;
		params[i].filterImage = filter_components;

		// Part of the array to be processed by the thread
		params[i].pixelInit = i * (filter_args.pixelCount / NUMBER_OF_THREADS);
		params[i].pixelEnd = (i + 1) * (filter_args.pixelCount / NUMBER_OF_THREADS);

		// If array size is not divisible by the number of threads, the last thread must process extra data
		if (i == NUMBER_OF_THREADS - 1){ // Get last thread

			while (filter_args.pixelCount - params[i].pixelEnd != 0){
				params[i].pixelEnd++; // Add rows to the last thread
			}
		}

		// Checking that the pixels to be calculated are not out of bounds
		if(params[i].pixelInit > filter_args.pixelCount || params[i].pixelEnd > filter_args.pixelCount){
			perror("Pixels out of bounds!!");
		}

		// Create thread and work
		if (pthread_create(&threads[i], NULL, FilterThread, &(params[i])) != 0){

			printf("ERROR creating thread: %d.\n", i);
			exit(EXIT_FAILURE);
		}
	}

	// Wait untill all threads are done
	for (uint i = 0; i < NUMBER_OF_THREADS; i++){
		pthread_join(threads[i], NULL);
	}
}

int main(){

	cimg::exception_mode(0);

    // Open file and object initialization
	CImg<data_t> srcImage;
	try
	{
		CImg<data_t> loadImage(SOURCE_IMG);
		srcImage = loadImage;
	}
	catch(CImgIOException& e){
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
	filter_image filter_components;
	uint widthFilter = filterImage.width();// Getting information from the Filter image
	uint heightFilter = filterImage.height();

	// Time variables
	struct timespec tStart, tEnd;
    double dElapsedTime;

	srcImage.display(); // Displays the source image
	uint width = srcImage.width();// Getting information from the source image
	uint height = srcImage.height();
	uint nComp = srcImage.spectrum();// source image number of components

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
		exit(EXIT_FAILURE);
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

	// ALGORITHM --> Repeated N times
	uint i;
	for(i = 0; i < REPEAT_ALGORITHM; i++){
		filterProcess(filter_args, filter_components);
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
