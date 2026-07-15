#pragma once
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/AssetManager.hpp"
#include "graphics_impl/img.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// מימוש נכס תמונה עבור ה-AssetManager המבוסס על ה-Img שלכם
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
    Img &m_screenCanvas;                       // קנבס הציור הראשי של המשחק
    std::string m_windowName;                  // שם החלון הפיזי - נדרש עבור imshow/getWindowProperty
    Vector2D m_logicalRange{1000.0f, 1000.0f}; // מרחב הקואורדינטות הלוגי
    AssetManager m_assetManager;

    // מטמון sprite מוקטן פר assetId, כדי לא לבצע cv::resize מחדש בכל פריים
    // כשגודל התא לא השתנה. הערך שמור יחד עם הגודל שביקש אותו, כך שאם
    // הגודל המבוקש משתנה (למשל שינוי גודל חלון) המטמון מתעדכן אוטומטית -
    // אין צורך בלוגיקת invalidation נפרדת. חל רק על הנתיב הנפוץ (בלי
    // סיבוב, בלי ROI חיתוך מ-sprite sheet).
    std::unordered_map<std::string, std::pair<Vector2D, Img>> m_spriteScaleCache;

    // המרת צבע לוגי ל-cv::Scalar של OpenCV (בפורמט BGRA/BGR)
    cv::Scalar toCvScalar(Color color) const
    {
        return cv::Scalar(color.b, color.g, color.r, color.a);
    }

    // תרגום קואורדינטה לוגית לנקודה פיזית על ה-cv::Mat
    cv::Point toPhysical(Vector2D logicalPos) const
    {
        Vector2D targetSize = getTargetSize();
        return cv::Point(
            static_cast<int>((logicalPos.x / m_logicalRange.x) * targetSize.x),
            static_cast<int>((logicalPos.y / m_logicalRange.y) * targetSize.y));
    }

    // תרגום מימדים לוגיים למימדים פיזיים
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

    void beginFrame() override
    {
        // הכנה לתחילת פריים (ניתן להוסיף לוגיקה במידת הצורך)
    }

    void clear(Color color) override
    {
        if (!m_screenCanvas.is_loaded())
        {
            return;
        }
        // צביעת המטריצה כולה בצבע הרקע שנבחר
        m_screenCanvas.mat().setTo(toCvScalar(color));
    }

    void endFrame() override
    {
        // סיום פריים (למשל חישובים סופיים לפני הצגה, אם יידרשו בעתיד)
    }

    void presentFrame() override
    {
        if (!m_screenCanvas.is_loaded())
            return;
        cv::imshow(m_windowName, m_screenCanvas.get_mat());
    }

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
        if (!m_screenCanvas.is_loaded())
            return;

        cv::Point topLeft = toPhysical(position);
        cv::Size physSize = sizeToPhysical(size);
        cv::Point bottomRight(topLeft.x + physSize.width, topLeft.y + physSize.height);

        int thickness = fill ? cv::FILLED : 1;
        cv::rectangle(m_screenCanvas.mat(), topLeft, bottomRight, toCvScalar(color), thickness, cv::LINE_AA);
    }

    void drawLine(Vector2D start, Vector2D end, Color color, float thickness) override
    {
        if (!m_screenCanvas.is_loaded())
            return;

        cv::Point p1 = toPhysical(start);
        cv::Point p2 = toPhysical(end);

        cv::line(m_screenCanvas.mat(), p1, p2, toCvScalar(color), static_cast<int>(thickness), cv::LINE_AA);
    }

    void drawCircle(Vector2D center, float radius, Color color, bool fill) override
    {
        if (!m_screenCanvas.is_loaded())
            return;

        cv::Point physCenter = toPhysical(center);
        Vector2D targetSize = getTargetSize();
        int r = static_cast<int>((radius / m_logicalRange.x) * targetSize.x);

        int thickness = fill ? cv::FILLED : 1;
        cv::circle(m_screenCanvas.mat(), physCenter, r, toCvScalar(color), thickness, cv::LINE_AA);
    }

    void drawSprite(std::string_view assetId,
                    Vector2D position,
                    Vector2D size,
                    float rotationDegrees,
                    const Vector2D *srcOffset,
                    const Vector2D *srcSize) override
    {
        if (!m_screenCanvas.is_loaded())
            return;

        try
        {
            auto &asset = m_assetManager.getAsset<ImgTextureAsset>(assetId);
            if (!asset.image.is_loaded())
                return;

            cv::Point physPos = toPhysical(position);
            cv::Size physSize = sizeToPhysical(size);

            Img spriteToDraw;
            if (srcOffset && srcSize)
            {
                // נתיב ה-Sprite Sheet/Atlas לא נכנס למטמון (פחות נפוץ, וגם
                // ה-ROI עצמו יכול להשתנות בין קריאות עם אותו assetId).
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
                // תיקון: הנתיב הנפוץ (תמונה שלמה, בלי סיבוב) - בדיקת מטמון
                // לפני ביצוע cv::resize. ברוב הפריימים גודל התא לא משתנה,
                // כך שברירת המחדל היא cache hit ולא resize חוזר על כל כלי.
                Vector2D requestedSize{static_cast<float>(physSize.width), static_cast<float>(physSize.height)};
                std::string cacheKey(assetId);

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

            // ביצוע סיבוב לתמונה במידה וצוין (rotationDegrees != 0) - תמיד
            // מחושב על עותק, כך שהמטמון עצמו אף פעם לא מכיל תמונה מסובבת.
            if (std::abs(rotationDegrees) > 0.01f)
            {
                cv::Mat rotated;
                cv::Point2f center(spriteToDraw.get_mat().cols / 2.0f, spriteToDraw.get_mat().rows / 2.0f);
                cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, -rotationDegrees, 1.0);
                cv::warpAffine(spriteToDraw.get_mat(), rotated, rotationMatrix, spriteToDraw.get_mat().size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
                spriteToDraw.mat() = rotated;
            }

            // ציור ה-Sprite על גבי קנבס המסך הראשי
            spriteToDraw.draw_on(m_screenCanvas, physPos.x, physPos.y);
        }
        catch (...)
        {
            // ציור מלבן ורוד חלופי (Fallback במקרה של שגיאה בטעינת הנכס)
            drawRectangle(position, size, {255, 0, 255, 255}, true);
        }
    }

    void drawText(std::string_view text, Vector2D position, int fontSize, Color color) override
    {
        if (!m_screenCanvas.is_loaded())
            return;

        cv::Point physPos = toPhysical(position);

        Vector2D targetSize = getTargetSize();
        double fontScale = (fontSize / 24.0) * (targetSize.x / m_logicalRange.x);

        m_screenCanvas.put_text(
            std::string(text),
            physPos.x,
            physPos.y,
            fontScale,
            toCvScalar(color),
            1);
    }

    Vector2D getTargetSize() const override
    {
        if (!m_screenCanvas.is_loaded())
        {
            return {800.0f, 800.0f}; // ברירת מחדל בטוחה
        }
        const cv::Mat &mat = m_screenCanvas.get_mat();
        return {static_cast<float>(mat.cols), static_cast<float>(mat.rows)};
    }

    AssetManager &getAssetManager()
    {
        return m_assetManager;
    }
};