#include "AsciiRenderer.hpp"
#include <iostream>

AsciiRenderer::AsciiRenderer(int width, int height)
    : m_width(width), m_height(height) {}

void AsciiRenderer::setDimensions(int width, int height) {
    m_width = width;
    m_height = height;
}

std::string AsciiRenderer::convertFrameToAscii(const cv::Mat& frame) const {
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(m_width, m_height), 0, 0, cv::INTER_AREA);

    const std::string chars = " .,-~:;=!*#$@";
    std::string output;
    output.reserve(static_cast<size_t>(m_width) * m_height * 20);

    int lastR = -1;
    int lastG = -1;
    int lastB = -1;

    for (int y = 0; y < resized.rows; ++y) {
        const auto* rowPtr = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < resized.cols; ++x) {
            const cv::Vec3b& pixel = rowPtr[x];
            int b = pixel[0];
            int g = pixel[1];
            int r = pixel[2];

            if (r != lastR || g != lastG || b != lastB) {
                output += "\x1b[38;2;";
                output += std::to_string(r);
                output += ';';
                output += std::to_string(g);
                output += ';';
                output += std::to_string(b);
                output += 'm';
                lastR = r;
                lastG = g;
                lastB = b;
            }

            int luminance = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
            int index = (luminance * static_cast<int>(chars.length() - 1)) / 255;
            output += chars[index];
        }
        output += '\n';
    }
    output += "\x1b[0m";
    return output;
}

void AsciiRenderer::renderAscii(const std::string& asciiFrame) const {
    std::cout << "\x1b[H" << asciiFrame << std::flush;
}

void AsciiRenderer::renderFrame(const cv::Mat& frame) {
    std::string ascii = convertFrameToAscii(frame);
    renderAscii(ascii);
}
