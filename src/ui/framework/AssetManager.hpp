// רכיב זה משתמש בטכניקה של מחיקת טיפוסים (Type Erasure) על ידי החזקת מצביעים למחלקת בסיס ריקה IAsset
// ה-AssetManager מאפשר לטעון כל משאב גרפי (כמו תמונות, גופנים או קטעי שמע) בצורה מנותקת,
// ומספק גישה אליהם באמצעות ביצוע המרה דינמית (dynamic_cast) לטיפוס הקונקרטי רק בזמן הציור ב-Backend
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <type_traits>

// מחלקת בסיס ריקה לייצוג משאב מופשט בזיכרון
class IAsset {
public:
    virtual ~IAsset() = default;
};

class AssetManager {
private:
    // מיפוי בין מזהה טקסטואלי ייחודי לבין המשאב המופשט שלו בזיכרון
    std::unordered_map<std::string, std::unique_ptr<IAsset>> m_assets;

public:
    AssetManager() = default;
    ~AssetManager() = default;

    // מניעת העתקה של מנהל הנכסים כדי למנוע כפילויות משאבים ובעיות בעלות על זיכרון
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    
    AssetManager(AssetManager&&) noexcept = default;
    AssetManager& operator=(AssetManager&&) noexcept = default;

    /**
     * טעינה ורישום של משאב חדש במערכת.
     * @tparam T סוג המשאב הקונקרטי (חייב לרשת מ-IAsset).
     * @param assetId המזהה הטקסטואלי הייחודי שנשתמש בו לגישה למשאב.
     * @param args הפרמטרים שיועברו לבנאי של המשאב הקונקרטי (למשל נתיב לקובץ).
     */
    template <typename T, typename... Args>
    void loadAsset(const std::string& assetId, Args&&... args) {
        static_assert(std::is_base_of_v<IAsset, T>, "T must derive from IAsset");
        
        if (m_assets.find(assetId) != m_assets.end()) {
            return; // המשאב כבר קיים בזיכרון, אין צורך לטעון שוב
        }

        // תיקון: בונים את האובייקט לפני ההכנסה למפה, במקום
        // m_assets[assetId] = std::make_unique<T>(...).
        // אם הבנייה זורקת חריגה (למשל קובץ תמונה חסר), חשוב שהמפה לא תכיל
        // entry ריק עבור assetId זה - אחרת הבדיקה למעלה תחשוב שהנכס "כבר
        // נטען" בכל קריאה עתידית, והנכס לא ייטען לעולם שוב באותה ריצה,
        // אפילו אם התיקון בפועל (למשל הוספת הקובץ החסר) כבר בוצע.
        auto asset = std::make_unique<T>(std::forward<Args>(args)...);
        m_assets.emplace(assetId, std::move(asset));
    }

    /**
     * שליפת משאב מהמערכת וביצוע המרה לטיפוס המבוקש.
     * @tparam T סוג המשאב הקונקרטי אליו אנו רוצים להמיר.
     * @param assetId המזהה הטקסטואלי של המשאב.
     */
    template <typename T>
    T& getAsset(std::string_view assetId) const {
        static_assert(std::is_base_of_v<IAsset, T>, "T must derive from IAsset");
        
        auto it = m_assets.find(std::string(assetId));
        if (it == m_assets.end()) {
            throw std::runtime_error("Asset not found: " + std::string(assetId));
        }

        auto* casted = dynamic_cast<T*>(it->second.get());
        if (!casted) {
            throw std::runtime_error("Invalid asset type cast for: " + std::string(assetId));
        }

        return *casted;
    }

    // הסרת משאב ספציפי מהזיכרון
    void unloadAsset(std::string_view assetId) {
        m_assets.erase(std::string(assetId));
    }

    // ניקוי מוחלט של כל המשאבים הטעונים
    void clear() {
        m_assets.clear();
    }
};