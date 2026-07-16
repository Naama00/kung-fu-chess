#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

class Img {
public:
    Img();
    
    // קבלת נתיב בצורה יעילה ללא העתקות מיותרות
    Img& read(const std::filesystem::path& path,
              const std::pair<int, int>& size = {},
              bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);
    
    void draw_on(Img& other_img, int x, int y);
    
    void put_text(std::string_view txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255),
                  int thickness = 1);
    
    void show();
    
    const cv::Mat& get_mat() const { return img; }
    cv::Mat& mat() { return img; }
    bool is_loaded() const { return !img.empty(); }

private:
    cv::Mat img;

    // משתני עזר (Buffers) למניעת הקצאות דינמיות בכל פריים מחדש
    std::vector<cv::Mat> m_srcPlanes;
    std::vector<cv::Mat> m_dstPlanes;
    cv::Mat m_alphaMask;
    cv::Mat m_alphaF;
    cv::Mat m_invAlphaF;
    cv::Mat m_blendedChannel;
};