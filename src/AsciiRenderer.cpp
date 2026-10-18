#include "AsciiRenderer.hpp"
#include <iostream>
#include <thread>
#include <algorithm>
#include <cstdint>

namespace {
    inline void appendU8(std::string& s, uint8_t val) {
        if (val >= 100) {
            s.push_back(static_cast<char>('0' + (val / 100)));
            val %= 100;
            s.push_back(static_cast<char>('0' + (val / 10)));
            s.push_back(static_cast<char>('0' + (val % 10)));
        } else if (val >= 10) {
            s.push_back(static_cast<char>('0' + (val / 10)));
            s.push_back(static_cast<char>('0' + (val % 10)));
        } else {
            s.push_back(static_cast<char>('0' + val));
        }
    }
}

AsciiRenderer::AsciiRenderer(int width, int height)
    : m_width(width), m_height(height) {}

void AsciiRenderer::setDimensions(int width, int height) {
    m_width = width;
    m_height = height;
}

std::string AsciiRenderer::convertFrameToAscii(const cv::Mat& frame) const {
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(m_width, m_height), 0, 0, cv::INTER_LINEAR);

    static constexpr char chars[] = " .,-~:;=!*#$@";
    constexpr int charCount = sizeof(chars) - 2; // length 13, max index 12

    std::string output;
    output.reserve(static_cast<size_t>(m_width) * m_height * 8);

    int lastR = -1;
    int lastG = -1;
    int lastB = -1;

    for (int y = 0; y < resized.rows; ++y) {
        const auto* rowPtr = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < resized.cols; ++x) {
            const cv::Vec3b& pixel = rowPtr[x];
            uint8_t b = pixel[0];
            uint8_t g = pixel[1];
            uint8_t r = pixel[2];

            if (r != lastR || g != lastG || b != lastB) {
                output.append("\x1b[38;2;");
                appendU8(output, r);
                output.push_back(';');
                appendU8(output, g);
                output.push_back(';');
                appendU8(output, b);
                output.push_back('m');
                lastR = r;
                lastG = g;
                lastB = b;
            }

            int luminance = (299 * r + 587 * g + 114 * b) / 1000;
            int index = (luminance * charCount) / 255;
            output.push_back(chars[index]);
        }
        output.push_back('\n');
    }
    output.append("\x1b[0m");
    return output;
}

void AsciiRenderer::convertBatchParallel(const std::vector<cv::Mat>& batch, std::vector<std::string>& outResults) const {
    size_t n = batch.size();
    outResults.resize(n);
    if (n == 0) return;

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    if (numThreads > n) numThreads = static_cast<unsigned int>(n);

    std::vector<std::jthread> workers;
    workers.reserve(numThreads);

    size_t chunkSize = (n + numThreads - 1) / numThreads;
    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, n);
        if (start >= end) break;

        workers.emplace_back([this, start, end, &batch, &outResults]() {
            for (size_t i = start; i < end; ++i) {
                outResults[i] = convertFrameToAscii(batch[i]);
            }
        });
    }
}

void AsciiRenderer::renderAscii(const std::string& asciiFrame) const {
    std::cout << "\x1b[H" << asciiFrame << std::flush;
}

void AsciiRenderer::renderFrame(const cv::Mat& frame) {
    std::string ascii = convertFrameToAscii(frame);
    renderAscii(ascii);
}
