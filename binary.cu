#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include "binary.cuh"

__global__ void binaryThresholdKernel(const unsigned char *input, unsigned char *output, int width, int height, int threshold)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height)
    {
        int idx = y * width + x;
        output[idx] = (input[idx] >= threshold) ? 255 : 0;
    }
}

__global__ void resizeKernel(const unsigned char *input, unsigned char *output,
                             int srcWidth, int srcHeight, int dstWidth, int dstHeight)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < dstWidth && y < dstHeight)
    {
        float scaleX = (float)srcWidth / dstWidth;
        float scaleY = (float)srcHeight / dstHeight;

        int srcX = (int)(x * scaleX);
        int srcY = (int)(y * scaleY);

        if (srcX >= srcWidth)
            srcX = srcWidth - 1;
        if (srcY >= srcHeight)
            srcY = srcHeight - 1;

        int srcIdx = srcY * srcWidth + srcX;
        int dstIdx = y * dstWidth + x;

        output[dstIdx] = input[srcIdx];
    }
}

__global__ void stitchKernel(const unsigned char *input, unsigned char *output,
                             int smallWidth, int smallHeight, int canvasWidth, int canvasHeight,
                             int rows, int cols)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < canvasWidth && y < canvasHeight)
    {
        int col = x / smallWidth;
        int row = y / smallHeight;

        if (col < cols && row < rows)
        {
            int smallX = x % smallWidth;
            int smallY = y % smallHeight;
            int smallIdx = smallY * smallWidth + smallX;
            int canvasIdx = y * canvasWidth + x;

            output[canvasIdx] = input[smallIdx];
        }
    }
}

__global__ void stitchMultipleKernel(const unsigned char *allInputs, unsigned char *output,
                                     int srcWidth, int srcHeight,
                                     int canvasWidth, int canvasHeight,
                                     int rows, int cols)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < canvasWidth && y < canvasHeight)
    {
        int cellWidth = canvasWidth / cols;
        int cellHeight = canvasHeight / rows;

        int col = x / cellWidth;
        int row = y / cellHeight;

        if (col < cols && row < rows)
        {
            int inputIdx = row * cols + col;

            // Calculate local coordinates in the cell
            int localX = x % cellWidth;
            int localY = y % cellHeight;

            // Map to source coordinates (nearest neighbor resize)
            // Using integer arithmetic for scale
            int srcX = (localX * srcWidth) / cellWidth;
            int srcY = (localY * srcHeight) / cellHeight;

            if (srcX >= srcWidth)
                srcX = srcWidth - 1;
            if (srcY >= srcHeight)
                srcY = srcHeight - 1;

            // Calculate offset in the big buffer
            // Each frame is srcWidth * srcHeight
            long long frameOffset = (long long)inputIdx * srcWidth * srcHeight;
            int pixelOffset = srcY * srcWidth + srcX;

            output[y * canvasWidth + x] = allInputs[frameOffset + pixelOffset];
        }
    }
}

__global__ void histogramKernel(const unsigned char *input, int *histogram, int size)
{
    __shared__ int temp[256];
    int tid = threadIdx.x;
    if (tid < 256)
        temp[tid] = 0;
    __syncthreads();

    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    while (i < size)
    {
        atomicAdd(&temp[input[i]], 1);
        i += stride;
    }
    __syncthreads();

    if (tid < 256)
    {
        atomicAdd(&histogram[tid], temp[tid]);
    }
}

void cudaBinaryThreshold(const unsigned char *input, unsigned char *output, int width, int height, int threshold)
{
    unsigned char *d_input;
    unsigned char *d_output;
    size_t size = width * height * sizeof(unsigned char);

    cudaMalloc(&d_input, size);
    cudaMalloc(&d_output, size);

    cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice);

    dim3 blockSize(16, 16);
    dim3 gridSize((width + blockSize.x - 1) / blockSize.x, (height + blockSize.y - 1) / blockSize.y);

    binaryThresholdKernel<<<gridSize, blockSize>>>(d_input, d_output, width, height, threshold);

    cudaMemcpy(output, d_output, size, cudaMemcpyDeviceToHost);

    cudaFree(d_input);
    cudaFree(d_output);
}

void cudaBinaryThresholdTriangle(const unsigned char *input, unsigned char *output, int width, int height)
{
    unsigned char *d_input;
    unsigned char *d_output;
    int *d_histogram;
    size_t size = width * height * sizeof(unsigned char);

    cudaMalloc(&d_input, size);
    cudaMalloc(&d_output, size);
    cudaMalloc(&d_histogram, 256 * sizeof(int));

    cudaMemcpy(d_input, input, size, cudaMemcpyHostToDevice);
    cudaMemset(d_histogram, 0, 256 * sizeof(int));

    // Calculate Histogram
    int numBlocks = 256;
    histogramKernel<<<numBlocks, 256>>>(d_input, d_histogram, width * height);

    int h[256];
    cudaMemcpy(h, d_histogram, 256 * sizeof(int), cudaMemcpyDeviceToHost);

    // Triangle Algorithm on Host
    int minBin = 0, maxBin = 255;
    while (minBin < 256 && h[minBin] == 0) minBin++;
    while (maxBin >= 0 && h[maxBin] == 0) maxBin--;

    int threshold = 128; // Default fallback
    if (maxBin > minBin)
    {
        int peak = minBin;
        int maxCount = h[minBin];
        for (int i = minBin + 1; i <= maxBin; i++)
        {
            if (h[i] > maxCount)
            {
                maxCount = h[i];
                peak = i;
            }
        }

        int start = peak;
        int end = maxBin;
        bool inverted = false;
        if ((peak - minBin) >= (maxBin - peak))
        {
            inverted = true;
            end = minBin;
        }

        float maxDist = -1.0f;
        double vx = end - start;
        double vy = h[end] - h[start];

        int step = (inverted) ? -1 : 1;
        // Loop from start to end
        for (int i = start; i != end; i += step)
        {
            double bx = i - start;
            double by = h[i] - h[start];
            double cross = vx * by - vy * bx;
            float dist = (float)((cross < 0) ? -cross : cross);

            if (dist > maxDist)
            {
                maxDist = dist;
                threshold = i;
            }
        }
        // If inverted, some implementations adjust threshold
        // We stick to the basic geometric definition
    }

    // Apply Threshold
    dim3 blockSize(16, 16);
    dim3 gridSize((width + blockSize.x - 1) / blockSize.x, (height + blockSize.y - 1) / blockSize.y);
    binaryThresholdKernel<<<gridSize, blockSize>>>(d_input, d_output, width, height, threshold);

    cudaMemcpy(output, d_output, size, cudaMemcpyDeviceToHost);

    cudaFree(d_input);
    cudaFree(d_output);
    cudaFree(d_histogram);
}

/// @brief resize an image using nearest neighbor interpolation.
/// @param input ptr to the input image data
/// @param output ptr to the output image data
/// @param srcWidth width of the input image
/// @param srcHeight height of the input image
/// @param dstWidth width of the output image
/// @param dstHeight height of the output image
void cudaResize(const unsigned char *input, unsigned char *output,
                int srcWidth, int srcHeight, int dstWidth, int dstHeight)
{
    unsigned char *d_input;                                        // ptr to the input data on the GPU
    unsigned char *d_output;                                       // ptr to the output data on the GPU
    size_t srcSize = srcWidth * srcHeight * sizeof(unsigned char); // size of the input image in bytes
    size_t dstSize = dstWidth * dstHeight * sizeof(unsigned char); // size of the output image in bytes

    cudaMalloc(&d_input, srcSize);  // allocate memory for the input image on the GPU
    cudaMalloc(&d_output, dstSize); // allocate memory for the output image on the GPU

    cudaMemcpy(d_input, input, srcSize, cudaMemcpyHostToDevice);//copy the input data from the CPU to the GPU

    dim3 blockSize(16, 16);//block size for the resize kernel
    dim3 gridSize((dstWidth + blockSize.x - 1) / blockSize.x, (dstHeight + blockSize.y - 1) / blockSize.y);//grid size for the resize kernel: (N+M-1)/M

    resizeKernel<<<gridSize, blockSize>>>(d_input, d_output, srcWidth, srcHeight, dstWidth, dstHeight);//launch the resize kernel on the GPU

    cudaMemcpy(output, d_output, dstSize, cudaMemcpyDeviceToHost);//copy the output data from the GPU to the CPU

    cudaFree(d_input);//free the memory for the input image on the GPU
    cudaFree(d_output);//free the memory for the output image on the GPU
}

void cudaStitch(const unsigned char *input, unsigned char *output,
                int smallWidth, int smallHeight, int canvasWidth, int canvasHeight,
                int rows, int cols)
{
    unsigned char *d_input;
    unsigned char *d_output;
    size_t smallSize = smallWidth * smallHeight * sizeof(unsigned char);
    size_t canvasSize = canvasWidth * canvasHeight * sizeof(unsigned char);

    cudaMalloc(&d_input, smallSize);
    cudaMalloc(&d_output, canvasSize);

    cudaMemcpy(d_input, input, smallSize, cudaMemcpyHostToDevice);
    cudaMemset(d_output, 0, canvasSize);

    dim3 blockSize(16, 16);
    dim3 gridSize((canvasWidth + blockSize.x - 1) / blockSize.x, (canvasHeight + blockSize.y - 1) / blockSize.y);

    stitchKernel<<<gridSize, blockSize>>>(d_input, d_output, smallWidth, smallHeight,
                                          canvasWidth, canvasHeight, rows, cols);

    cudaMemcpy(output, d_output, canvasSize, cudaMemcpyDeviceToHost);

    cudaFree(d_input);
    cudaFree(d_output);
}

void cudaStitchMultiple(unsigned char **inputs, int numInputs,
                        unsigned char *output,
                        int srcWidth, int srcHeight,
                        int canvasWidth, int canvasHeight,
                        int rows, int cols)
{
    unsigned char *d_allInputs;
    unsigned char *d_output;
    size_t srcFrameSize = srcWidth * srcHeight * sizeof(unsigned char);
    size_t totalInputSize = numInputs * srcFrameSize;
    size_t canvasSize = canvasWidth * canvasHeight * sizeof(unsigned char);

    cudaMalloc(&d_allInputs, totalInputSize);
    cudaMalloc(&d_output, canvasSize);

    // Copy all inputs to device buffer
    for (int i = 0; i < numInputs; ++i)
    {
        cudaMemcpy(d_allInputs + i * srcFrameSize, inputs[i], srcFrameSize, cudaMemcpyHostToDevice);
    }

    cudaMemset(d_output, 0, canvasSize);

    dim3 blockSize(16, 16);
    dim3 gridSize((canvasWidth + blockSize.x - 1) / blockSize.x, (canvasHeight + blockSize.y - 1) / blockSize.y);

    stitchMultipleKernel<<<gridSize, blockSize>>>(d_allInputs, d_output, srcWidth, srcHeight, canvasWidth, canvasHeight, rows, cols);

    cudaMemcpy(output, d_output, canvasSize, cudaMemcpyDeviceToHost);

    cudaFree(d_allInputs);
    cudaFree(d_output);
}
