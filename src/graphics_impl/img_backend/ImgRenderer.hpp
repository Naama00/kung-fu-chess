#pragma once
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/AssetManager.hpp"
#include "img.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

class ImgTextureAsset : public IAsset
{
public:
    Img image;

    explicit ImgTextureAsset(const std::string &filePath)
    {
        image.read(filePath);
    }
};

class ImgRenderer : public IRenderer
{
private:
    Img &m_screenCanvas;
    std::string m_windowName;
    Vector2D m_logicalRange{1000.0f, 1000.0f};
    AssetManager m_assetManager;

    // שיפור מפתח המטמון: שימוש בכתובת המצביע (pointer address) של ה-Asset.
    // זה מונע הקצאות והשוואת מחרוזות יקרה בכל פריים עבור כל כלי שחמט!
    std::unordered_map<const IAsset*, std::pair<Vector2D, Img>> m_spriteScaleCache;

    struct LetterboxTransform {
        float padX  = 0.0f;
        float padY  = 0.0f;
        float scale = 1.0f;
        int   winW  = 0;
        int   winH  = 0;
    };
    LetterboxTransform m_letterbox;

    cv::Scalar toCvScalar(Color color) const
    {
        return cv::Scalar(color.b, color.g, color.r, color.a);
    }

    cv::Point toPhysical(Vector2D logicalPos) const
    {
        Vector2D targetSize = getTargetSize();
        return cv::Point(
            static_cast<int>((logicalPos.x / m_logicalRange.x) * targetSize.x),
            static_cast<int>((logicalPos.y / m_logicalRange.y) * targetSize.y));
    }

    cv::Size sizeToPhysical(Vector2D logicalSize) const
    {
        Vector2D targetSize = getTargetSize();
        return cv::Size(
            static_cast<int>((logicalSize.x / m_logicalRange.x) * targetSize.x),
            static_cast<int>((logicalSize.y / m_logicalRange.y) * targetSize.y));
    }

public:
    explicit ImgRenderer(Img &screenCanvas, std::string windowName)
        : m_screenCanvas(screenCanvas), m_windowName(std::move(windowName)) {}

    // פונקציה לניקוי יזום של ה-cache בעת מעבר בין מסכים
    void clearCache() {
        m_spriteScaleCache.clear();
    }

    void beginFrame() override {}

    void clear(Color color) override
    {
        if (!m_screenCanvas.is_loaded()) return;
        m_screenCanvas.mat().setTo(toCvScalar(color));
    }

    void endFrame() override {}

    void presentFrame() override
    {
        if (!m_screenCanvas.is_loaded()) return;

        const cv::Mat& content = m_screenCanvas.get_mat();
        int contentW = content.cols;
        int contentH = content.rows;

        cv::Rect winRect = cv::getWindowImageRect(m_windowName);
        int winW = winRect.width;
        int winH = winRect.height;

        if (winW <= 0 || winH <= 0) {
            cv::imshow(m_windowName, content);
            return;
        }

        float scale = std::min(static_cast<float>(winW) / contentW,
                               static_cast<float>(winH) / contentH);
        int scaledW = static_cast<int>(contentW * scale);
        int scaledH = static_cast<int>(contentH * scale);
        int padX = (winW - scaledW) / 2;
        int padY = (winH - scaledH) / 2;

        m_letterbox = { static_cast<float>(padX), static_cast<float>(padY), scale, winW, winH };

        cv::Mat scaled;
        cv::resize(content, scaled, cv::Size(scaledW, scaledH), 0, 0, cv::INTER_LINEAR);
        cv::Mat frame(winH, winW, content.type(), cv::Scalar(0, 0, 0, 255));
        scaled.copyTo(frame(cv::Rect(padX, padY, scaledW, scaledH)));
        cv::imshow(m_windowName, frame);
    }

    const LetterboxTransform& getLetterboxTransform() const { return m_letterbox; }

    bool isWindowOpen() const override
    {
        try
        {
            double prop = cv::getWindowProperty(m_windowName, cv::WND_PROP_AUTOSIZE);
            return prop >= 0;
        }
        catch (const cv::Exception &)
        {
            return false;
        }
    }

    void drawRectangle(Vector2D position, Vector2D size, Color color, bool fill) override
    {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point topLeft = toPhysical(position);
        cv::Size physSize = sizeToPhysical(size);
        cv::Point bottomRight(topLeft.x + physSize.width, topLeft.y + physSize.height);

        int thickness = fill ? cv::FILLED : 1;
        cv::rectangle(m_screenCanvas.mat(), topLeft, bottomRight, toCvScalar(color), thickness, cv::LINE_AA);
    }

    void drawLine(Vector2D start, Vector2D end, Color color, float thickness) override
    {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point p1 = toPhysical(start);
        cv::Point p2 = toPhysical(end);

        cv::line(m_screenCanvas.mat(), p1, p2, toCvScalar(color), static_cast<int>(thickness), cv::LINE_AA);
    }

    void drawCircle(Vector2D center, float radius, Color color, bool fill) override
    {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point physCenter = toPhysical(center);
        Vector2D targetSize = getTargetSize();
        int r = static_cast<int>((radius / m_logicalRange.x) * targetSize.x);

        int thickness = fill ? cv::FILLED : 1;
        cv::circle(m_screenCanvas.mat(), physCenter, r, toCvScalar(color), thickness, cv::LINE_AA);
    }

     void drawSector(Vector2D center, float radius, float startAngle, float endAngle, Color color, bool fill) override
    {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point physCenter = toPhysical(center);
        Vector2D targetSize = getTargetSize();
        int r = static_cast<int>((radius / m_logicalRange.x) * targetSize.x);

        int thickness = fill ? cv::FILLED : 1;

        // cv::ellipse ב-OpenCV מקבלת זוויות במעלות ומציירת סקטור/קשת בצורה חלקה
        cv::ellipse(m_screenCanvas.mat(), physCenter, cv::Size(r, r), 0.0,
                    static_cast<double>(startAngle), static_cast<double>(endAngle),
                    toCvScalar(color), thickness, cv::LINE_AA);
    }

    void drawSprite(std::string_view assetId,
                    Vector2D position,
                    Vector2D size,
                    float rotationDegrees,
                    const Vector2D *srcOffset,
                    const Vector2D *srcSize) override
    {
        if (!m_screenCanvas.is_loaded()) return;

        try
        {
            auto &asset = m_assetManager.getAsset<ImgTextureAsset>(assetId);
            if (!asset.image.is_loaded()) return;

            cv::Point physPos = toPhysical(position);
            cv::Size physSize = sizeToPhysical(size);

            Img spriteToDraw;
            if (srcOffset && srcSize)
            {
                const cv::Mat &srcMat = asset.image.get_mat();
                cv::Rect roi(
                    static_cast<int>(srcOffset->x),
                    static_cast<int>(srcOffset->y),
                    static_cast<int>(srcSize->x),
                    static_cast<int>(srcSize->y));

                cv::Mat cutMat = srcMat(roi).clone();
                cv::resize(cutMat, cutMat, physSize, 0, 0, cv::INTER_LINEAR);

                Img croppedImg;
                croppedImg.mat() = cutMat;
                spriteToDraw = croppedImg;
            }
            else
            {
                Vector2D requestedSize{static_cast<float>(physSize.width), static_cast<float>(physSize.height)};
                
                // שיפור: שימוש בכתובת המצביע של האובייקט בתוך ה-Cache במקום מחרוזת
                const IAsset* cacheKey = &asset;

                auto cacheIt = m_spriteScaleCache.find(cacheKey);
                bool cacheHit = cacheIt != m_spriteScaleCache.end() &&
                                 cacheIt->second.first.x == requestedSize.x &&
                                 cacheIt->second.first.y == requestedSize.y;

                if (cacheHit)
                {
                    spriteToDraw = cacheIt->second.second;
                }
                else
                {
                    cv::Mat scaledMat;
                    cv::resize(asset.image.get_mat(), scaledMat, physSize, 0, 0, cv::INTER_LINEAR);
                    Img scaledImg;
                    scaledImg.mat() = scaledMat;
                    m_spriteScaleCache[cacheKey] = {requestedSize, scaledImg};
                    spriteToDraw = scaledImg;
                }
            }

            if (std::abs(rotationDegrees) > 0.01f)
            {
                cv::Mat rotated;
                cv::Point2f center(spriteToDraw.get_mat().cols / 2.0f, spriteToDraw.get_mat().rows / 2.0f);
                cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, -rotationDegrees, 1.0);
                cv::warpAffine(spriteToDraw.get_mat(), rotated, rotationMatrix, spriteToDraw.get_mat().size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
                spriteToDraw.mat() = rotated;
            }

            spriteToDraw.draw_on(m_screenCanvas, physPos.x, physPos.y);
        }
        catch (...)
        {
            drawRectangle(position, size, {255, 0, 255, 255}, true);
        }
    }

    void drawText(std::string_view text, Vector2D position, int fontSize, Color color) override
    {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point physPos = toPhysical(position);
        Vector2D targetSize = getTargetSize();
        double fontScale = (fontSize / 24.0) * (targetSize.x / m_logicalRange.x);

        m_screenCanvas.put_text(text, physPos.x, physPos.y, fontScale, toCvScalar(color), 1);
    }

    Vector2D getTargetSize() const override
    {
        if (!m_screenCanvas.is_loaded())
        {
            return {800.0f, 800.0f};
        }
        const cv::Mat &mat = m_screenCanvas.get_mat();
        return {static_cast<float>(mat.cols), static_cast<float>(mat.rows)};
    }

    AssetManager &getAssetManager()
    {
        return m_assetManager;
    }
};