#include "graphics_impl/img.hpp"
#include <iostream>
#include <stdexcept>

Img::Img() {
    // Constructor - img is automatically initialized as empty
}

Img& Img::read(const std::string& path,
               const std::pair<int, int>& size,
               bool keep_aspect,
               int interpolation) {
    img = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        throw std::runtime_error("Cannot load image: " + path);
    }

    if (size.first != 0 && size.second != 0) {  // Check if size is not empty
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

    // Handle different channel counts
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
        // Alpha-blend BGRA source onto BGR/BGRA destination per-pixel.
        // Split source into B, G, R, A planes (each is an h×w single-channel Mat).
        std::vector<cv::Mat> srcPlanes;
        cv::split(source_img, srcPlanes);

        // Build a float alpha mask in [0,1] with the same spatial size as roi.
        cv::Mat alphaMask;
        srcPlanes[3].convertTo(alphaMask, CV_32F, 1.0 / 255.0);

        // Split the destination ROI into its own channel planes.
        std::vector<cv::Mat> dstPlanes;
        cv::split(roi, dstPlanes);

        // Blend each colour channel: dst = (1-a)*dst + a*src
        for (int c = 0; c < 3; ++c) {
            cv::Mat srcF, dstF;
            srcPlanes[c].convertTo(srcF, CV_32F);
            dstPlanes[c].convertTo(dstF, CV_32F);

            cv::Mat blended = (1.0f - alphaMask).mul(dstF) + alphaMask.mul(srcF);
            blended.convertTo(dstPlanes[c], CV_8U);
        }

        // If the destination has an alpha channel too, keep it fully opaque.
        if (dstPlanes.size() == 4) {
            dstPlanes[3].setTo(255);
        }

        cv::merge(dstPlanes, roi);
    } else {
        // Direct copy for BGR images (no transparency).
        source_img.copyTo(roi);
    }
}

void Img::put_text(const std::string& txt, int x, int y, double font_size,
                   const cv::Scalar& color, int thickness) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::putText(img, txt, cv::Point(x, y),
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