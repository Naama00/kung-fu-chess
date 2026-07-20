#include "img.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

Img::Img() {}

Img& Img::read(const std::filesystem::path& path,
               const std::pair<int, int>& size,
               bool keep_aspect,
               int interpolation) {
    img = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        throw std::runtime_error("Cannot load image: " + path.string());
    }

    if (size.first != 0 && size.second != 0) {
        int target_w = size.first;
        int target_h = size.second;
        int h = img.rows;
        int w = img.cols;

        if (keep_aspect) {
            double scale = std::min(static_cast<double>(target_w) / w, 
                                   static_cast<double>(target_h) / h);
            int new_w = static_cast<int>(w * scale);
            int new_h = static_cast<int>(h * scale);
            cv::resize(img, img, cv::Size(new_w, new_h), 0, 0, interpolation);
        } else {
            cv::resize(img, img, cv::Size(target_w, target_h), 0, 0, interpolation);
        }
    }

    return *this;
}

void Img::draw_on(Img& other_img, int x, int y) {
    if (img.empty() || other_img.img.empty()) {
        throw std::runtime_error("Both images must be loaded before drawing.");
    }

    if (x < 0 || y < 0) {
        throw std::runtime_error("Draw position cannot be negative (x=" +
                                  std::to_string(x) + ", y=" + std::to_string(y) + ").");
    }

    cv::Mat source_img = img;
    cv::Mat target_img = other_img.img;
    
    if (source_img.channels() != target_img.channels()) {
        if (source_img.channels() == 3 && target_img.channels() == 4) {
            cv::cvtColor(source_img, source_img, cv::COLOR_BGR2BGRA);
        } else if (source_img.channels() == 4 && target_img.channels() == 3) {
            cv::cvtColor(source_img, source_img, cv::COLOR_BGRA2BGR);
        }
    }

    int h = source_img.rows;
    int w = source_img.cols;
    int H = target_img.rows;
    int W = target_img.cols;

    if (y + h > H || x + w > W) {
        throw std::runtime_error("Image does not fit at the specified position.");
    }

    cv::Mat roi = target_img(cv::Rect(x, y, w, h));

    if (source_img.channels() == 4) {
        // Split source channels (reusing the existing vector to avoid reallocation)
        cv::split(source_img, m_srcPlanes);

        // Create an alpha mask in the range [0, 1]
        m_srcPlanes[3].convertTo(m_alphaF, CV_32F, 1.0 / 255.0);
        
        // Create the inverted mask (1.0f - alpha) using built-in optimization
        cv::subtract(1.0f, m_alphaF, m_invAlphaF);

        // Split target channels
        cv::split(roi, m_dstPlanes);

        // blending optimized per channel
        for (int c = 0; c < 3; ++c) {
            cv::Mat srcF, dstF;
            m_srcPlanes[c].convertTo(srcF, CV_32F);
            m_dstPlanes[c].convertTo(dstF, CV_32F);

            // dst = dst * (1 - alpha) + src * alpha
            cv::multiply(dstF, m_invAlphaF, dstF);
            cv::multiply(srcF, m_alphaF, srcF);
            cv::add(dstF, srcF, m_blendedChannel);

            m_blendedChannel.convertTo(m_dstPlanes[c], CV_8U);
        }

        if (m_dstPlanes.size() == 4) {
            m_dstPlanes[3].setTo(255);
        }

        cv::merge(m_dstPlanes, roi);
    } else {
        source_img.copyTo(roi);
    }
}

void Img::put_text(std::string_view txt, int x, int y, double font_size,
                   const cv::Scalar& color, int thickness) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::putText(img, std::string(txt), cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX, font_size,
                color, thickness, cv::LINE_AA);
}

void Img::show() {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::imshow("Image", img);
    cv::waitKey(0);
    cv::destroyAllWindows();
}