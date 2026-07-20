// server/DatabaseManager.hpp
#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <unordered_map>

namespace kungfu {

class DatabaseManager {
private:
    struct UserRecord {
        std::string passwordHash;
        int rating = 1200;
    };

    std::unordered_map<std::string, UserRecord> users_;
    std::mutex dbMutex_;

    // פונקציית עזר לגיבוב בסיסי של סיסמאות (לשמירה על פרטיות)
    std::string hashPassword(const std::string& password) const;

public:
    DatabaseManager() = default;
    ~DatabaseManager();

    // מניעת העתקה
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // פתיחת קובץ ה-DB ויצירת טבלאות
    bool initialize(const std::string& dbPath);

    // רישום משתמש חדש (ELO ברירת מחדל: 1200)
    bool registerUser(const std::string& username, const std::string& password);

    // אימות משתמש (מחזיר true אם הפרטים נכונים ושולף את ה-rating הנוכחי שלו)
    bool authenticateUser(const std::string& username, const std::string& password, int& outRating);

    // עדכון מדד ה-Rating
    bool updateRating(const std::string& username, int newRating);
};

} // namespace kungfu