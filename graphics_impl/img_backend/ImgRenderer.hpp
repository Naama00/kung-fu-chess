#pragma once
#include "../../ui/IRenderer.hpp"
#include "../../ui/AssetManager.hpp"
#include "img.hpp" // קובץ ה-Header שלכם
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <string_view>

// מימוש נכס תמונה עבור ה-AssetManager המבוסס על ה-Img שלכם
class ImgTextureAsset : public IAsset {
public:
    Img image;
    
    explicit ImgTextureAsset(const std::string& filePath) {
        image.read(filePath);
    }
};

class ImgRenderer : public IRenderer {
private:
    Img& m_screenCanvas;                       // קנבס הציור הראשי של המשחק
    Vector2D m_logicalRange{1000.0f, 1000.0f}; // מרחב הקואורדינטות הלוגי
    AssetManager m_assetManager;

    // המרת צבע לוגי ל-cv::Scalar של OpenCV (בפורמט BGRA/BGR)
    cv::Scalar toCvScalar(Color color) const {
        return cv::Scalar(color.b, color.g, color.r, color.a);
    }

    // תרגום קואורדינטה לוגית לנקודה פיזית על ה-cv::Mat
    cv::Point toPhysical(Vector2D logicalPos) const {
        Vector2D targetSize = getTargetSize();
        return cv::Point(
            static_cast<int>((logicalPos.x / m_logicalRange.x) * targetSize.x),
            static_cast<int>((logicalPos.y / m_logicalRange.y) * targetSize.y)
        );
    }

    // תרגום מימדים לוגיים למימדים פיזיים
    cv::Size sizeToPhysical(Vector2D logicalSize) const {
        Vector2D targetSize = getTargetSize();
        return cv::Size(
            static_cast<int>((logicalSize.x / m_logicalRange.x) * targetSize.x),
            static_cast<int>((logicalSize.y / m_logicalRange.y) * targetSize.y)
        );
    }

public:
    explicit ImgRenderer(Img& screenCanvas) : m_screenCanvas(screenCanvas) {}

    void beginFrame() override {
        // הכנה לתחילת פריים (ניתן להוסיף לוגיקה במידת הצורך)
    }

    void clear(Color color) override {
        if (!m_screenCanvas.is_loaded()) {
            return;
        }
        // צביעת המטריצה כולה בצבע הרקע שנבחר
        cv::Mat& mat = const_cast<cv::Mat&>(m_screenCanvas.get_mat());
        mat.setTo(toCvScalar(color));
    }

    void endFrame() override {
        // סיום פריים (למשל רענון חלון או כתיבה לדיסק)
    }

    void drawRectangle(Vector2D position, Vector2D size, Color color, bool fill) override {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point topLeft = toPhysical(position);
        cv::Size physSize = sizeToPhysical(size);
        cv::Point bottomRight(topLeft.x + physSize.width, topLeft.y + physSize.height);

        cv::Mat& mat = const_cast<cv::Mat&>(m_screenCanvas.get_mat());
        int thickness = fill ? cv::FILLED : 1;

        cv::rectangle(mat, topLeft, bottomRight, toCvScalar(color), thickness, cv::LINE_AA);
    }

    void drawLine(Vector2D start, Vector2D end, Color color, float thickness) override {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point p1 = toPhysical(start);
        cv::Point p2 = toPhysical(end);

        cv::Mat& mat = const_cast<cv::Mat&>(m_screenCanvas.get_mat());
        
        cv::line(mat, p1, p2, toCvScalar(color), static_cast<int>(thickness), cv::LINE_AA);
    }

    void drawCircle(Vector2D center, float radius, Color color, bool fill) override {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point physCenter = toPhysical(center);
        cv::Size physRadiusSize = sizeToPhysical({radius, 0.0f});
        int r = physRadiusSize.width;

        cv::Mat& mat = const_cast<cv::Mat&>(m_screenCanvas.get_mat());
        int thickness = fill ? cv::FILLED : 1;

        cv::circle(mat, physCenter, r, toCvScalar(color), thickness, cv::LINE_AA);
    }

    void drawSprite(std::string_view assetId, 
                    Vector2D position, 
                    Vector2D size, 
                    float rotationDegrees,
                    const Vector2D* srcOffset,
                    const Vector2D* srcSize) override {
        if (!m_screenCanvas.is_loaded()) return;

        try {
            auto& asset = m_assetManager.getAsset<ImgTextureAsset>(assetId);
            if (!asset.image.is_loaded()) return;

            cv::Point physPos = toPhysical(position);
            cv::Size physSize = sizeToPhysical(size);

            // במקרה שנדרשת תת-תמונה (Sprite Sheet / Atlas)
            Img spriteToDraw;
            if (srcOffset && srcSize) {
                // חיתוך האזור הרלוונטי ממטריצת המקור
                const cv::Mat& srcMat = asset.image.get_mat();
                cv::Rect roi(
                    static_cast<int>(srcOffset->x), 
                    static_cast<int>(srcOffset->y), 
                    static_cast<int>(srcSize->x), 
                    static_cast<int>(srcSize->y)
                );
                
                // יצירת אובייקט Img זמני עבור ה-ROI וחשיפת המטריצה שלו
                cv::Mat cutMat = srcMat(roi).clone();
                cv::resize(cutMat, cutMat, physSize, 0, 0, cv::INTER_LINEAR);
                
                // שימוש ב-Reflection / פריצה קלה כדי לעקוף את מנגנון ה-Read
                Img croppedImg;
                cv::Mat& croppedMat = const_cast<cv::Mat&>(croppedImg.get_mat());
                croppedMat = cutMat;
                spriteToDraw = croppedImg;
            } else {
                // לקיחת התמונה המלאה ושינוי גודלה לגודל הפיזי הנדרש
                spriteToDraw = asset.image;
                cv::Mat scaledMat;
                cv::resize(spriteToDraw.get_mat(), scaledMat, physSize, 0, 0, cv::INTER_LINEAR);
                cv::Mat& refMat = const_cast<cv::Mat&>(spriteToDraw.get_mat());
                refMat = scaledMat;
            }

            // ביצוע סיבוב לתמונה במידה וצוין (rotationDegrees != 0)
            if (std::abs(rotationDegrees) > 0.01f) {
                cv::Mat rotated;
                cv::Point2f center(spriteToDraw.get_mat().cols / 2.0f, spriteToDraw.get_mat().rows / 2.0f);
                cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, -rotationDegrees, 1.0);
                cv::warpAffine(spriteToDraw.get_mat(), rotated, rotationMatrix, spriteToDraw.get_mat().size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0,0,0,0));
                cv::Mat& refMat = const_cast<cv::Mat&>(spriteToDraw.get_mat());
                refMat = rotated;
            }

            // ציור ה-Sprite על גבי קנבס המסך הראשי
            spriteToDraw.draw_on(m_screenCanvas, physPos.x, physPos.y);

        } catch (...) {
            // ציור מלבן ורוד חלופי (Fallback במקרה של שגיאה בטעינת הנכס)
            drawRectangle(position, size, {255, 0, 255, 255}, true);
        }
    }

    void drawText(std::string_view text, Vector2D position, int fontSize, Color color) override {
        if (!m_screenCanvas.is_loaded()) return;

        cv::Point physPos = toPhysical(position);
        
        // התאמת גודל הגופן של OpenCV (Scale) ביחס לגודל הלוגי המבוקש
        double fontScale = fontSize / 24.0; 
        
        m_screenCanvas.put_text(
            std::string(text), 
            physPos.x, 
            physPos.y, 
            fontScale, 
            toCvScalar(color), 
            1
        );
    }

    Vector2D getTargetSize() const override {
        if (!m_screenCanvas.is_loaded()) {
            return { 800.0f, 800.0f }; // ברירת מחדל בטוחה
        }
        const cv::Mat& mat = m_screenCanvas.get_mat();
        return { static_cast<float>(mat.cols), static_cast<float>(mat.rows) };
    }

    AssetManager& getAssetManager() {
        return m_assetManager;
    }
};