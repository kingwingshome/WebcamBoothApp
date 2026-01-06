#ifndef BINARY_CUH
#define BINARY_CUH

#ifdef __cplusplus
extern "C" {
#endif

void cudaBinaryThreshold(const unsigned char* input, unsigned char* output, int width, int height, int threshold);
void cudaBinaryThresholdTriangle(const unsigned char* input, unsigned char* output, int width, int height);
void cudaResize(const unsigned char* input, unsigned char* output, int srcWidth, int srcHeight, int dstWidth, int dstHeight);
void cudaStitch(const unsigned char* input, unsigned char* output, int smallWidth, int smallHeight, int canvasWidth, int canvasHeight, int rows, int cols);
void cudaStitchMultiple(unsigned char** inputs, int numInputs, unsigned char* output, int srcWidth, int srcHeight, int canvasWidth, int canvasHeight, int rows, int cols);

#ifdef __cplusplus
}
#endif

#endif
