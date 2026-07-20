// ui/framework/EventBus.hpp
#pragma once
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace kungfu {

class EventBus {
private:
    // מחלקת בסיס לצורך מחיקת טיפוסים (Type Erasure) של המנויים
    struct SubscriptionBase {
        virtual ~SubscriptionBase() = default;
    };

    // מימוש קונקרטי המחזיק את ה-Callback הטיפוסי
    template <typename EventType>
    struct Subscription : public SubscriptionBase {
        std::function<void(const EventType&)> callback;
        Subscription(std::function<void(const EventType&)> cb) : callback(std::move(cb)) {}
    };

    // מפה המקשרת בין מזהה טיפוס האירוע לבין רשימת המנויים הרשומים אליו
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<SubscriptionBase>>> subscribers_;
    mutable std::mutex mutex_;

public:
    EventBus() = default;
    ~EventBus() = default;

    // מניעת העתקה לשמירה על שלמות היישות
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * הרשמה (Subscribe) לקבלת אירועים מטיפוס מסוים.
     * @tparam EventType מבנה הנתונים (struct) של האירוע המבוקש.
     * @param callback פונקציה אנונימית (או מתודה) שתופעל עם קבלת האירוע.
     */
    template <typename EventType>
    void subscribe(std::function<void(const EventType&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto sub = std::make_shared<Subscription<EventType>>(std::move(callback));
        subscribers_[std::type_index(typeid(EventType))].push_back(sub);
    }

    /**
     * פרסום (Publish) של אירוע לכל המנויים הרלוונטיים.
     * @tparam EventType מבנה הנתונים (struct) של האירוע המפורסם.
     * @param event פרטי האירוע שנשלחים למנויים.
     */
    template <typename EventType>
    void publish(const EventType& event) {
        std::vector<std::shared_ptr<SubscriptionBase>> subs_to_notify;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(std::type_index(typeid(EventType)));
            if (it != subscribers_.end()) {
                // העתקה מהירה של המצביעים כדי לשחרר את ה-Mutex בהקדם.
                // מונע Deadlocks במקרה שמנוי מנסה להירשם/לבטל רישום מתוך ה-Callback עצמו.
                subs_to_notify = it->second;
            }
        }

        for (const auto& sub_base : subs_to_notify) {
            if (auto sub = std::static_pointer_cast<Subscription<EventType>>(sub_base)) {
                if (sub->callback) {
                    sub->callback(event);
                }
            }
        }
    }
};

} // namespace kungfu