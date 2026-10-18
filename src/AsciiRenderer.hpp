#pragma once
#include <opencv2/opencv.hpp>
#include <string>

class AsciiRenderer {
public:
    explicit AsciiRenderer(int width = 120, int height = 40);

    void setDimensions(int width, int height);
    [[nodiscard]] int getWidth() const { return m_width; }
    [[nodiscard]] int getHeight() const { return m_height; }

    [[nodiscard]] std::string convertFrameToAscii(const cv::Mat& frame) const;
    void renderAscii(const std::string& asciiFrame) const;
    void renderFrame(const cv::Mat& frame);

private:
    int m_width;
    int m_height;
};
