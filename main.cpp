#include <opencv2/opencv.hpp>
#include <iostream>
#include "binary.cuh"
#include "photobooth.h"
#include <cuda_runtime.h>
#include <cstdlib>
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string saveFileDialog()
{
    OPENFILENAMEA ofn;
    char szFile[260];

    // Generate timestamp for default filename
    auto now = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "webcam_booth_%Y%m%d_%H%M%S.png");
    std::string defaultName = oss.str();

    // Initialize szFile with default name
    strncpy_s(szFile, defaultName.c_str(), sizeof(szFile) - 1);

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    // ofn.lpstrFile[0] = '\0'; // Removed to keep default filename
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files\0*.png\0JPEG Files\0*.jpg\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE)
    {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

enum DisplayMode
{
    NORMAL,
    OPENCV_BINARY,
    CUDA_BINARY,
    CUDA_BINARY_THRESHOLD_TRIANGLE,
    PHOTOBOOTH_MODE
};

static bool isCudaAvailable()
{
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return err == cudaSuccess && count > 0;
}

int main(int argc, char **argv)
{
    std::string windowName = "Webcam Booth";
    int webcamId = 0;
    if (argc >= 2)
    {
        webcamId = std::atoi(argv[1]);
        if (webcamId < 0)
            webcamId = 0;
    }
    cv::VideoCapture cap(webcamId);

    if (!cap.isOpened())
    {
        std::cerr << "Error: Cannot open webcam" << std::endl;
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);

    DisplayMode mode = NORMAL;
    bool cudaAvailable = isCudaAvailable();

    std::cout << "Webcam Booth started (id=" << webcamId << ")" << std::endl;
    std::cout << "Press 'B' to toggle OpenCV binary mode" << std::endl;
    std::cout << "Press 'C' to toggle CUDA binary mode" << std::endl;
    std::cout << "Press 'T' to toggle CUDA binary threshold Triangle mode" << std::endl;
    std::cout << "Press 'P' to toggle PhotoBooth mode" << std::endl;
    std::cout << "  In PhotoBooth mode: Use 'A' (prev) and 'D' (next) to switch effects" << std::endl;
    std::cout << "Press 'SPACE' to switch to normal mode" << std::endl;
    std::cout << "Press 'S' to capture a photo" << std::endl;
    std::cout << "Press 'ESC' to exit" << std::endl;

    cv::namedWindow(windowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(windowName, 1920, 1080);

    std::vector<EffectType> allEffects = PhotoBooth::getAllEffects();
    int currentEffectIdx = allEffects.size() - 1;

    while (true)
    {
        cv::Mat frame;
        cap >> frame;

        if (frame.empty())
        {
            std::cerr << "Error: Cannot capture frame" << std::endl;
            break;
        }

        cv::Mat displayFrame;
        std::string modeText;

        if (mode == CUDA_BINARY)
        {

            std::vector<cv::Mat> frames(9);
            for (int i = 0; i < 9; i++)
            {
                cv::cvtColor(frame, frames[i], cv::COLOR_BGR2GRAY);
                cudaBinaryThreshold(frames[i].data, frames[i].data, frames[i].cols, frames[i].rows, 128);
            }

            cv::Mat canvas(1080, 1920, CV_8UC1, cv::Scalar(0));

            int smallWidth = 640;
            int smallHeight = 360;

            std::vector<unsigned char *> framePtrs(9);
            for (int i = 0; i < 9; i++)
            {
                framePtrs[i] = frames[i].data;
            }

            cudaStitchMultiple(framePtrs.data(), 9, canvas.data,
                               frames[0].cols, frames[0].rows,
                               canvas.cols, canvas.rows, 3, 3);

            cv::cvtColor(canvas, displayFrame, cv::COLOR_GRAY2BGR);
            modeText = "Mode: CUDA Binary";
        }
        else if (mode == CUDA_BINARY_THRESHOLD_TRIANGLE)
        {
            std::vector<cv::Mat> frames(9);
            for (int i = 0; i < 9; i++)
            {
                cv::cvtColor(frame, frames[i], cv::COLOR_BGR2GRAY);
                cudaBinaryThresholdTriangle(frames[i].data, frames[i].data, frames[i].cols, frames[i].rows);
            }

            cv::Mat canvas(1080, 1920, CV_8UC1, cv::Scalar(0));

            int smallWidth = 640;
            int smallHeight = 360;

            std::vector<unsigned char *> framePtrs(9);
            for (int i = 0; i < 9; i++)
            {
                framePtrs[i] = frames[i].data;
            }

            cudaStitchMultiple(framePtrs.data(), 9, canvas.data,
                               frames[0].cols, frames[0].rows,
                               canvas.cols, canvas.rows, 3, 3);

            cv::cvtColor(canvas, displayFrame, cv::COLOR_GRAY2BGR);
            modeText = "Mode: CUDA Binary Threshold Triangle";
        }
        else if (mode == PHOTOBOOTH_MODE)
        {
            EffectType currentEffect = allEffects[currentEffectIdx];
            PhotoBooth::applyEffect(frame, displayFrame, currentEffect);
            modeText = "Mode: PhotoBooth - " + PhotoBooth::getEffectName(currentEffect);
        }
        else
        {
            std::vector<cv::Mat> frames(9);
            int thresholdValue = 128;
            std::vector<cv::ThresholdTypes> thresholdTypes = {
                cv::THRESH_BINARY, cv::THRESH_BINARY_INV,
                cv::THRESH_TRUNC, cv::THRESH_TOZERO, cv::THRESH_TOZERO_INV,
                cv::THRESH_OTSU, cv::THRESH_TRIANGLE};
            for (int i = 0; i < 9; i++)
            {
                if (mode == OPENCV_BINARY)
                {
                    if (i == 0)
                    {
                        frames[i] = frame.clone();
                        continue;
                    }
                    if (i == 8)
                    {
                        cv::cvtColor(frame, frames[i], cv::COLOR_BGR2GRAY);
                        cv::adaptiveThreshold(frames[i], frames[i], 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 7, 2);
                        cv::cvtColor(frames[i], frames[i], cv::COLOR_GRAY2BGR);
                        continue;
                    }
                    cv::cvtColor(frame, frames[i], cv::COLOR_BGR2GRAY);
                    cv::threshold(frames[i], frames[i], thresholdValue, 255, thresholdTypes[i - 1]);
                    cv::cvtColor(frames[i], frames[i], cv::COLOR_GRAY2BGR);
                }
                else
                {
                    frames[i] = frame.clone();
                    if (i > 0)
                        cv::applyColorMap(frames[i], frames[i], i - 1);
                }
            }

            cv::Mat canvas(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));

            int smallWidth = 640;
            int smallHeight = 360;

            for (int row = 0; row < 3; row++)
            {
                for (int col = 0; col < 3; col++)
                {
                    int idx = row * 3 + col;
                    cv::Mat smallFrame;
                    cv::resize(frames[idx], smallFrame, cv::Size(smallWidth, smallHeight));
                    int x = col * smallWidth;
                    int y = row * smallHeight;
                    cv::Rect roi(x, y, smallWidth, smallHeight);
                    smallFrame.copyTo(canvas(roi));
                }
            }

            displayFrame = canvas;

            if (mode == OPENCV_BINARY)
            {
                modeText = "Mode: OpenCV Binary";
            }
            else
            {
                modeText = "Mode: Normal";
            }
        }
        
        cv::Mat saveFrame = displayFrame.clone();
        cv::putText(displayFrame, modeText, cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

        cv::imshow(windowName, displayFrame);

        int key = cv::waitKey(1);

        if (key == 27)
        {
            break;
        }
        else if (key == 's' || key == 'S')
        {
            std::string savePath = saveFileDialog();
            if (!savePath.empty())
            {
                // Simple check to append .png if no extension provided
                if (savePath.find('.') == std::string::npos)
                {
                    savePath += ".png";
                }
                cv::imwrite(savePath, saveFrame);
                std::cout << "Saved photo to: " << savePath << std::endl;
            }
        }
        else if (key == 'b' || key == 'B')
        {
            mode = OPENCV_BINARY;
            std::cout << "OpenCV binary mode enabled" << std::endl;
        }
        else if (key == 'c' || key == 'C')
        {
            if (cudaAvailable)
            {
                mode = CUDA_BINARY;
                std::cout << "CUDA binary mode enabled" << std::endl;
            }
            else
            {
                std::cout << "CUDA device not available" << std::endl;
            }
        }
        else if (key == 't' || key == 'T')
        {
            mode = CUDA_BINARY_THRESHOLD_TRIANGLE;
            std::cout << "CUDA binary threshold triangle mode enabled" << std::endl;
        }
        else if (key == 'p' || key == 'P')
        {
            mode = PHOTOBOOTH_MODE;
            std::cout << "PhotoBooth mode enabled" << std::endl;
        }
        else if (key == 32)
        {
            mode = NORMAL;
            std::cout << "Normal mode enabled" << std::endl;
        }

        if (mode == PHOTOBOOTH_MODE)
        {
            if (key == 'a' || key == 'A')
            {
                currentEffectIdx = (currentEffectIdx - 1 + allEffects.size()) % allEffects.size();
                std::cout << "Effect: " << PhotoBooth::getEffectName(allEffects[currentEffectIdx]) << std::endl;
            }
            else if (key == 'd' || key == 'D')
            {
                currentEffectIdx = (currentEffectIdx + 1) % allEffects.size();
                std::cout << "Effect: " << PhotoBooth::getEffectName(allEffects[currentEffectIdx]) << std::endl;
            }
        }
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}
