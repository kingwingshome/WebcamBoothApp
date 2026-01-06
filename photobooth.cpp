#include "photobooth.h"
#include <vector>
#include <cmath>
#include <algorithm>

static cv::Mat g_mapX, g_mapY;
static EffectType g_lastEffect = EffectType::NORMAL;
static cv::Size g_lastSize;

static void generateMap(EffectType type, cv::Size size, cv::Mat& mapX, cv::Mat& mapY);
static void remapWrapper(const cv::Mat& src, cv::Mat& dst, cv::Mat& mapX, cv::Mat& mapY);

void PhotoBooth::applyEffect(const cv::Mat& src, cv::Mat& dst, EffectType type) {
    if (src.empty()) return;

    bool isDeformation = (type == EffectType::BULGE || type == EffectType::DENT ||
                          type == EffectType::TWIRL || type == EffectType::SQUEEZE ||
                          type == EffectType::LIGHT_TUNNEL || type == EffectType::FISH_EYE ||
                          type == EffectType::STRETCH || type == EffectType::MIRROR || 
                          type == EffectType::POP_ART);

    if (isDeformation) {
        if (type != g_lastEffect || src.size() != g_lastSize) {
            generateMap(type, src.size(), g_mapX, g_mapY);
            g_lastEffect = type;
            g_lastSize = src.size();
        }
        remapWrapper(src, dst, g_mapX, g_mapY);
        
        if (type == EffectType::POP_ART) {
            cv::Mat gray;
            cv::cvtColor(dst, gray, cv::COLOR_BGR2GRAY);
            
            int halfW = dst.cols / 2;
            int halfH = dst.rows / 2;
            
            cv::Mat q1 = dst(cv::Rect(0, 0, halfW, halfH));
            cv::Mat q2 = dst(cv::Rect(halfW, 0, halfW, halfH));
            cv::Mat q3 = dst(cv::Rect(0, halfH, halfW, halfH));
            cv::Mat q4 = dst(cv::Rect(halfW, halfH, halfW, halfH));
            
            cv::Mat g1 = gray(cv::Rect(0, 0, halfW, halfH));
            cv::Mat g2 = gray(cv::Rect(halfW, 0, halfW, halfH));
            cv::Mat g3 = gray(cv::Rect(0, halfH, halfW, halfH));
            cv::Mat g4 = gray(cv::Rect(halfW, halfH, halfW, halfH));
            
            cv::applyColorMap(g1, q1, cv::COLORMAP_AUTUMN);
            cv::applyColorMap(g2, q2, cv::COLORMAP_BONE);
            cv::applyColorMap(g3, q3, cv::COLORMAP_SUMMER);
            cv::applyColorMap(g4, q4, cv::COLORMAP_OCEAN);
        }
        
        return;
    }

    switch (type) {
        case EffectType::SEPIA: applySepia(src, dst); break;
        case EffectType::BLACK_AND_WHITE: applyBlackAndWhite(src, dst); break;
        case EffectType::PLASTIC_CAMERA: applyPlasticCamera(src, dst); break;
        case EffectType::COMIC_BOOK: applyComicBook(src, dst); break;
        case EffectType::COLOR_PENCIL: applyColorPencil(src, dst); break;
        case EffectType::GLOW: applyGlow(src, dst); break;
        case EffectType::THERMAL_CAMERA: applyThermalCamera(src, dst); break;
        case EffectType::X_RAY: applyXRay(src, dst); break;
        // case EffectType::POP_ART: applyPopArt(src, dst); break;
        // case EffectType::MIRROR: applyMirror(src, dst); break;
        // POP_ART and MIRROR handled above
        case EffectType::ALL_EFFECTS_GRID: applyAllEffectsGrid(src, dst); break;
        case EffectType::NORMAL: 
        default: src.copyTo(dst); break;
    }
}

std::string PhotoBooth::getEffectName(EffectType type) {
    switch (type) {
        case EffectType::NORMAL: return "Normal";
        case EffectType::SEPIA: return "Sepia";
        case EffectType::BLACK_AND_WHITE: return "Black & White";
        case EffectType::PLASTIC_CAMERA: return "Plastic Camera";
        case EffectType::COMIC_BOOK: return "Comic Book";
        case EffectType::COLOR_PENCIL: return "Color Pencil";
        case EffectType::GLOW: return "Glow";
        case EffectType::THERMAL_CAMERA: return "Thermal Camera";
        case EffectType::X_RAY: return "X-Ray";
        case EffectType::POP_ART: return "Pop Art";
        case EffectType::BULGE: return "Bulge";
        case EffectType::DENT: return "Dent";
        case EffectType::TWIRL: return "Twirl";
        case EffectType::SQUEEZE: return "Squeeze";
        case EffectType::MIRROR: return "Mirror";
        case EffectType::LIGHT_TUNNEL: return "Light Tunnel";
        case EffectType::FISH_EYE: return "Fish Eye";
        case EffectType::STRETCH: return "Stretch";
        case EffectType::ALL_EFFECTS_GRID: return "All Effects Grid";
        default: return "Unknown";
    }
}

std::vector<EffectType> PhotoBooth::getAllEffects() {
    return {
        EffectType::NORMAL,
        EffectType::SEPIA, EffectType::BLACK_AND_WHITE, EffectType::PLASTIC_CAMERA,
        EffectType::COMIC_BOOK, EffectType::COLOR_PENCIL, EffectType::GLOW,
        EffectType::THERMAL_CAMERA, EffectType::X_RAY, EffectType::POP_ART,
        EffectType::BULGE, EffectType::DENT, EffectType::TWIRL,
        EffectType::SQUEEZE, EffectType::MIRROR, EffectType::LIGHT_TUNNEL,
        EffectType::FISH_EYE, EffectType::STRETCH, EffectType::ALL_EFFECTS_GRID
    };
}

void PhotoBooth::applySepia(const cv::Mat& src, cv::Mat& dst) {
    cv::transform(src, dst, cv::Matx33f(
        0.272, 0.534, 0.131,
        0.349, 0.686, 0.168,
        0.393, 0.769, 0.189));
}

void PhotoBooth::applyBlackAndWhite(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(gray, dst, cv::COLOR_GRAY2BGR);
}

void PhotoBooth::applyPlasticCamera(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat temp;
    src.convertTo(temp, -1, 1.2, 10); 
    
    int rows = src.rows;
    int cols = src.cols;
    cv::Mat vignette(rows, cols, CV_32F);
    
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            double dx = x - cols / 2.0;
            double dy = y - rows / 2.0;
            double radius = std::sqrt(dx * dx + dy * dy);
            double max_radius = std::sqrt(cols * cols + rows * rows) / 2.0;
            double factor = 1.0 - (radius / max_radius) * 0.6;
            vignette.at<float>(y, x) = (float)std::max(0.0, std::min(1.0, factor));
        }
    }
    
    std::vector<cv::Mat> channels;
    cv::split(temp, channels);
    for (int i = 0; i < 3; i++) {
        cv::Mat c;
        channels[i].convertTo(c, CV_32F);
        cv::multiply(c, vignette, c);
        c.convertTo(channels[i], CV_8U);
    }
    cv::merge(channels, dst);
}

void PhotoBooth::applyComicBook(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat gray, edges;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, gray, 7);
    cv::adaptiveThreshold(gray, edges, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 9, 2);
    
    cv::Mat color;
    cv::bilateralFilter(src, color, 9, 75, 75);
    
    cv::bitwise_and(color, color, dst, edges);
}

void PhotoBooth::applyColorPencil(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat gray, gray_inv, gray_blur, dst_gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::bitwise_not(gray, gray_inv);
    cv::GaussianBlur(gray_inv, gray_blur, cv::Size(21, 21), 0);
    
    dst_gray.create(gray.size(), CV_8UC1);
    for (int y = 0; y < gray.rows; y++) {
        for (int x = 0; x < gray.cols; x++) {
            int g = gray.at<uchar>(y, x);
            int b = gray_blur.at<uchar>(y, x);
            if (b == 255) dst_gray.at<uchar>(y, x) = 255;
            else dst_gray.at<uchar>(y, x) = std::min(255, (g * 256) / (255 - b));
        }
    }
    cv::cvtColor(dst_gray, dst, cv::COLOR_GRAY2BGR);
}

void PhotoBooth::applyGlow(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat blur;
    cv::GaussianBlur(src, blur, cv::Size(15, 15), 0);
    cv::addWeighted(src, 1.2, blur, 0.8, 0, dst);
}

void PhotoBooth::applyThermalCamera(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::applyColorMap(gray, dst, cv::COLORMAP_JET);
}

void PhotoBooth::applyXRay(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::bitwise_not(gray, gray);
    cv::cvtColor(gray, dst, cv::COLOR_GRAY2BGR);
}

void PhotoBooth::applyPopArt(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat small;
    cv::resize(src, small, cv::Size(src.cols/2, src.rows/2));
    
    cv::Mat canvas(src.size(), src.type());
    
    cv::Mat q1 = canvas(cv::Rect(0, 0, small.cols, small.rows));
    cv::Mat q2 = canvas(cv::Rect(small.cols, 0, small.cols, small.rows));
    cv::Mat q3 = canvas(cv::Rect(0, small.rows, small.cols, small.rows));
    cv::Mat q4 = canvas(cv::Rect(small.cols, small.rows, small.cols, small.rows));
    
    cv::Mat gray;
    cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);
    
    cv::applyColorMap(gray, q1, cv::COLORMAP_AUTUMN);
    cv::applyColorMap(gray, q2, cv::COLORMAP_BONE);
    cv::applyColorMap(gray, q3, cv::COLORMAP_SUMMER);
    cv::applyColorMap(gray, q4, cv::COLORMAP_OCEAN);
    
    dst = canvas;
}

void PhotoBooth::applyMirror(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat temp = src.clone();
    int cols = src.cols;
    cv::Mat left = temp(cv::Rect(0, 0, cols/2, src.rows));
    cv::Mat right = temp(cv::Rect(cols/2, 0, cols/2, src.rows));
    cv::flip(left, right, 1);
    dst = temp;
}

void PhotoBooth::applyAllEffectsGrid(const cv::Mat& src, cv::Mat& dst) {
    // 18 effects to display (excluding Grid itself)
    // Grid layout: 6 columns x 3 rows
    int rows = 3;
    int cols = 6;
    
    int cellWidth = src.cols / cols;
    int cellHeight = src.rows / rows;
    
    // Resize source once to cell size for efficiency
    cv::Mat smallSrc;
    cv::resize(src, smallSrc, cv::Size(cellWidth, cellHeight));
    
    dst = cv::Mat::zeros(src.size(), src.type());
    
    std::vector<EffectType> effects = getAllEffects();
    // Remove the last one (ALL_EFFECTS_GRID)
    if (!effects.empty()) effects.pop_back();
    
    for (size_t i = 0; i < effects.size() && i < (size_t)(rows * cols); ++i) {
        int r = i / cols;
        int c = i % cols;
        
        cv::Rect roi(c * cellWidth, r * cellHeight, cellWidth, cellHeight);
        cv::Mat cellDst = dst(roi);
        
        // Apply effect to small source
        // Note: applyEffect handles map generation internally.
        // For deformation effects, it will regenerate maps because size/effect changes.
        // This is acceptable for 18 small images.
        applyEffect(smallSrc, cellDst, effects[i]);
    }
}


static void remapWrapper(const cv::Mat& src, cv::Mat& dst, cv::Mat& mapX, cv::Mat& mapY) {
    cv::remap(src, dst, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
}

static void generateMap(EffectType type, cv::Size size, cv::Mat& mapX, cv::Mat& mapY) {
    int rows = size.height;
    int cols = size.width;
    mapX.create(rows, cols, CV_32F);
    mapY.create(rows, cols, CV_32F);
    
    float cx = cols / 2.0f;
    float cy = rows / 2.0f;
    float rMax = std::min(cx, cy);

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float r = std::sqrt(dx*dx + dy*dy);
            float theta = std::atan2(dy, dx);
            
            float newX = x, newY = y;

            if (type == EffectType::BULGE) {
                float strength = 0.5f;
                if (r < rMax) {
                    float rNew = r * (1 - strength * std::sin(3.14159f * r / (2 * rMax)));
                    newX = cx + rNew * std::cos(theta);
                    newY = cy + rNew * std::sin(theta);
                }
            } else if (type == EffectType::DENT) {
                float strength = 0.5f;
                if (r < rMax) {
                    float rNew = r * (1 + strength * std::sin(3.14159f * r / (2 * rMax)));
                    newX = cx + rNew * std::cos(theta);
                    newY = cy + rNew * std::sin(theta);
                }
            } else if (type == EffectType::TWIRL) {
                float alpha = 5.0f;
                if (r < rMax) {
                    float newTheta = theta + alpha * (1 - r / rMax);
                    newX = cx + r * std::cos(newTheta);
                    newY = cy + r * std::sin(newTheta);
                }
            } else if (type == EffectType::SQUEEZE) {
                float newDx = (std::abs(dx) < cx) ? std::copysign(std::pow(std::abs(dx)/cx, 0.5) * cx, dx) : dx;
                newX = cx + newDx;
            } else if (type == EffectType::LIGHT_TUNNEL) {
                float rNew = r * r / std::max(cx, cy); 
                newX = cx + rNew * std::cos(theta);
                newY = cy + rNew * std::sin(theta);
            } else if (type == EffectType::FISH_EYE) {
                 float ndx = (x - cx) / cx;
                 float ndy = (y - cy) / cy;
                 float nr = std::sqrt(ndx*ndx + ndy*ndy);
                 if (nr < 1.0) {
                    float rNew = std::asin(nr) * 2.0 / 3.14159; 
                    if (nr > 0.001) {
                        newX = cx + (rNew/nr * ndx) * cx;
                        newY = cy + (rNew/nr * ndy) * cy;
                    }
                 }
            } else if (type == EffectType::STRETCH) {
                float newDx = std::copysign(std::pow(std::abs(dx)/cx, 2.0) * cx, dx);
                newX = cx + newDx;
            } else if (type == EffectType::MIRROR) {
                newX = (x < cx) ? x : (cols - 1 - x);
            } else if (type == EffectType::POP_ART) {
                // Map each quadrant to the full image
                // Quadrants: 0: TL, 1: TR, 2: BL, 3: BR
                // x range: 0..cx (TL/BL), cx..cols (TR/BR)
                // y range: 0..cy (TL/TR), cy..rows (BL/BR)
                
                // Effective source coordinates should cover the full image
                // So we map 0..cx -> 0..cols, cx..cols -> 0..cols
                // Scale factor 2.0
                
                float sx = (x < cx) ? (x * 2.0f) : ((x - cx) * 2.0f);
                float sy = (y < cy) ? (y * 2.0f) : ((y - cy) * 2.0f);
                
                newX = sx;
                newY = sy;
            }

            mapX.at<float>(y, x) = newX;
            mapY.at<float>(y, x) = newY;
        }
    }
}
