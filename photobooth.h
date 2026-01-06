#ifndef PHOTOBOOTH_H
#define PHOTOBOOTH_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

enum class EffectType {
    NORMAL,
    // Style Filters
    SEPIA,
    BLACK_AND_WHITE,
    PLASTIC_CAMERA,
    COMIC_BOOK,
    COLOR_PENCIL,
    GLOW,
    THERMAL_CAMERA,
    X_RAY,
    POP_ART,
    
    // Deformation Effects
    BULGE,
    DENT,
    TWIRL,
    SQUEEZE,
    MIRROR,
    LIGHT_TUNNEL,
    FISH_EYE,
    STRETCH,
    ALL_EFFECTS_GRID
};

class PhotoBooth {
public:
    static void applyEffect(const cv::Mat& src, cv::Mat& dst, EffectType type);
    static std::string getEffectName(EffectType type);
    static std::vector<EffectType> getAllEffects();

private:
    // Helper functions for specific effects
    static void applySepia(const cv::Mat& src, cv::Mat& dst);
    static void applyBlackAndWhite(const cv::Mat& src, cv::Mat& dst);
    static void applyPlasticCamera(const cv::Mat& src, cv::Mat& dst);
    static void applyComicBook(const cv::Mat& src, cv::Mat& dst);
    static void applyColorPencil(const cv::Mat& src, cv::Mat& dst);
    static void applyGlow(const cv::Mat& src, cv::Mat& dst);
    static void applyThermalCamera(const cv::Mat& src, cv::Mat& dst);
    static void applyXRay(const cv::Mat& src, cv::Mat& dst);
    static void applyPopArt(const cv::Mat& src, cv::Mat& dst);
    static void applyMirror(const cv::Mat& src, cv::Mat& dst);
    static void applyAllEffectsGrid(const cv::Mat& src, cv::Mat& dst);
};

#endif // PHOTOBOOTH_H
