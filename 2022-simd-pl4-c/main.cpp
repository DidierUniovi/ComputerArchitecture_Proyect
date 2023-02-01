/*
 * Main.cpp
 *
 *  Created on: Fall 2019
 */

#include <stdio.h>
#include <immintrin.h> // Required to use intrinsic functions
#include <math.h>
#include <CImg.h>
#include <time.h>

using namespace cimg_library;

// Data type for image components
typedef float data_t;

const char* SOURCE_IMG = "bailarina.bmp";
const char* FILTER_IMG = "background_V.bmp";
const char* DESTINATION_IMG = "bailarina2.bmp";
const uint REPEAT_ALGORITHM = 60;

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

#define ITEMS_PER_PACKET (sizeof(__m256)/sizeof(data_t))

const __m256 V0  = _mm256_set1_ps(0.0f);
const __m256 V255 = _mm256_set1_ps(255.0f);
const __m256 V2 = _mm256_set1_ps(2.0f);

void blendColor(int pixelCount, int nPackets, data_t *c, data_t *srcColor, data_t* filterColor, data_t* destColor) {
	__m256 vSource, vFilter, va, vb;

	for (int i = 0; i < nPackets; i++) {
        vSource = _mm256_loadu_ps((srcColor + ITEMS_PER_PACKET * i));
        vFilter = _mm256_loadu_ps((filterColor + ITEMS_PER_PACKET * i));
        va = _mm256_sub_ps(V255, vFilter);
        vb = _mm256_mul_ps(V2, vSource);
        vb = _mm256_div_ps(vb, V255);
        vb = _mm256_mul_ps(vb, va);
        vb = _mm256_add_ps(vFilter, vb);
        va = _mm256_div_ps(vFilter, V255);
		va = _mm256_mul_ps(va, vb);

		#ifdef CHECK_COLOR_SATURATION
		va = _mm256_min_ps(va, V255); // Clamp value to assure that we do not have color saturation
		va = _mm256_max_ps(va, V0);
		#endif
        
		*(__m256 *)(c + ITEMS_PER_PACKET * i) = va;
    }
	// We want to differentiate the last iteration if we have excess data
	if (((pixelCount * sizeof(data_t))%sizeof(__m256)) != 0) { // Check if we have excess data smaller than a packet
		*(__m256 *)(c + ITEMS_PER_PACKET * nPackets) = _mm256_set1_ps(-1);
        int dataInExcess = (pixelCount)%(sizeof(__m256)/sizeof(data_t));
		vSource = _mm256_loadu_ps((srcColor + nPackets*ITEMS_PER_PACKET - (ITEMS_PER_PACKET- dataInExcess)));
        vFilter = _mm256_loadu_ps((filterColor + nPackets*ITEMS_PER_PACKET - (ITEMS_PER_PACKET- dataInExcess)));
        va = _mm256_sub_ps(V255, vFilter);
        vb = _mm256_mul_ps(V2, vSource);
        vb = _mm256_div_ps(vb, V255);
        vb = _mm256_mul_ps(vb, va);
        vb = _mm256_add_ps(vFilter, vb);
        va = _mm256_div_ps(vFilter, V255);
		va = _mm256_mul_ps(va, vb);

		#ifdef CHECK_COLOR_SATURATION
		va = _mm256_min_ps(va, V255); // Clamp value to assure that we do not have color saturation
		va = _mm256_max_ps(va, V0);
		#endif

		__m256i vi;
    	vi = _mm256_set1_epi64x(-1);
    	_mm256_maskstore_ps((data_t *)(c + nPackets*ITEMS_PER_PACKET - (ITEMS_PER_PACKET- dataInExcess)),vi, va); 
	}
    memcpy(destColor, c, pixelCount * sizeof(data_t)); // Copy the memory from c to the color channel of the final image
}

void filter (filter_args_t args0, filter_Image args1) {
	// Calculate the number of packets
    int nPackets = (args0.pixelCount * sizeof(data_t)/sizeof(__m256));
	data_t *c;

	if (((args0.pixelCount * sizeof(data_t))%sizeof(__m256)) != 0)
		c = (data_t *)_mm_malloc(sizeof(__m256) * (nPackets+1), sizeof(__m256));
	else
		c = (data_t *)_mm_malloc(sizeof(__m256) * nPackets, sizeof(__m256));
	
	blendColor(args0.pixelCount, nPackets, c, args0.pRsrc, args1.pRfilter, args0.pRdst);
	blendColor(args0.pixelCount, nPackets, c, args0.pGsrc, args1.pGfilter, args0.pGdst);
	blendColor(args0.pixelCount, nPackets, c, args0.pBsrc, args1.pBfilter, args0.pBdst);

    _mm_free(c); // Free the memory that was being used by c
}

int main() {
	// Open file and object initialization
	cimg::exception_mode(0);
	CImg<data_t> srcImage;
	try {
		CImg<data_t> loadImage(SOURCE_IMG);
		srcImage = loadImage;
	} catch (const CImgIOException& e) {
		perror("Failed to open the source image");
		exit(-2);
	}
	filter_args_t filter_args;
	data_t *pDstImage; // Pointer to the new image pixels

	// Filter Image initialization
	CImg<data_t> filterImage;
	try {
		CImg<data_t> loadImage(FILTER_IMG);
		filterImage = loadImage;
	} catch (const CImgIOException& e) {
		perror("Failed to open the filter image");
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
	if (clock_gettime(CLOCK_REALTIME, &tStart) == -1) {
		perror("Clock time error");
		exit(EXIT_FAILURE);
	}

	// ALGORITHM
	for(uint i = 0; i < REPEAT_ALGORITHM; i++){
		filter(filter_args, filter_components);
	}

	// Measuring end time
	if (clock_gettime(CLOCK_REALTIME, &tEnd) == -1) {
		perror("Clock time error");
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
