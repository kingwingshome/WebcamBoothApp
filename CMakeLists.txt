cmake_minimum_required(VERSION 3.18)
project(webcambooth_app LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

set(CUDA_TOOLKIT_ROOT_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8")
set(CMAKE_CUDA_ARCHITECTURES "52;60;61;70;75;80;86")

set(OpenCV_DIR "C:/WS_LIBS/opencv-4.8.0/build")
find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} main.cpp binary.cu photobooth.cpp)

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS} comdlg32)
