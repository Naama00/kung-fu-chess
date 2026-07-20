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
    // Base class for type erasure of subscribers
    struct SubscriptionBase {
        virtual ~SubscriptionBase() = default;
    };

    // Concrete implementation holding the typed callback
    template <typename EventType>
    struct Subscription : public SubscriptionBase {
        std::function<void(const EventType&)> callback;
        Subscription(std::function<void(const EventType&)> cb) : callback(std::move(cb)) {}
    };

    // Map linking event type identifiers to the list of registered subscribers
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<SubscriptionBase>>> subscribers_;
    mutable std::mutex mutex_;

public:
    EventBus() = default;
    ~EventBus() = default;

    // Prevent copying to preserve object integrity
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * Subscribe to receive events of a given type.
     * @tparam EventType The event data structure (struct).
     * @param callback An anonymous function (or method) that will be invoked when the event is received.
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
                // Quickly copy the pointers to release the mutex promptly.
                // Prevents deadlocks if a subscriber tries to subscribe/unsubscribe from inside the callback itself.
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