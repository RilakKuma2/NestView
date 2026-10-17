#pragma once
#include <atomic>
#include <memory>

// A worker owns its lease until it actually returns, even if its progress
// dialog or the originating window was already closed.
class NestBackgroundActivity {
public:
    NestBackgroundActivity() { count().fetch_add(1); }
    ~NestBackgroundActivity() { count().fetch_sub(1); }
    NestBackgroundActivity(const NestBackgroundActivity &) = delete;
    NestBackgroundActivity &operator=(const NestBackgroundActivity &) = delete;
    static bool active() { return count().load() != 0; }
    static std::shared_ptr<NestBackgroundActivity> begin() {
        return std::make_shared<NestBackgroundActivity>();
    }
private:
    static std::atomic_uint &count() { static std::atomic_uint value{0}; return value; }
};
