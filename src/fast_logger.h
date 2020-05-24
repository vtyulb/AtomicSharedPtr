#pragma once

#if not FAST_LOGGING_ENABLED
#define FAST_LOG(a, b) (static_cast<void>(0))
#else
#define FAST_LOG(a, b) LFStructs::FastLogger::Instance().push(a, b)

#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
#include <deque>

namespace LFStructs {

static uint64_t rdtsc() {
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

struct Operation {
    enum Type {
        Get        = 0,
        GetRefSucc = 1,
        GetRefAbrt = 2,
        CompareAndSwap = 3,
        CASFin     = 4,
        CASDestructed = 5,
        CASAbrt    = 6,
        Destruct   = 7,
        Push       = 9,
        Pop        = 10,
        GetInCAS   = 12,

        Ref        = 50,
        Unref      = 51,

        ObjectCreated = 100,
        ObjectDestroyed = 101
    } type;

    size_t address;
    size_t time;

    Operation() = default;
    inline Operation(Type t, size_t a, size_t time = rdtsc())
        : type(t)
        , address(a)
        , time(time)
    {}
};

class FastLogger {
    struct Storage {
        std::mutex lock;
        std::vector<std::pair<std::thread::id, std::vector<Operation>*>> containers;
    };

    const int MAX_LOG_COUNT = 2048;

public:
    static FastLogger &Instance() {
        thread_local FastLogger logger;
        return logger;
    }

    ~FastLogger() {
        std::lock_guard guard{storage.lock};
        for (auto i = storage.containers.begin(); i != storage.containers.end(); i++) {
            if (i->first == std::this_thread::get_id()) {
                storage.containers.erase(i);
                return;
            }
        }
        assert(false);
    }

    // smallest clock cycle diff I ever captured on one thread - 24
    // 36 clock cycle is a common number between two calls
    // 100-200 clock cycles to capture something usefull
    // 700-1600 cycle cycles to capture atomic operation under load
    void push(Operation::Type t, size_t address) {
        data[currentIndex] = Operation(t, address);
        currentIndex += 1;
        currentIndex %= MAX_LOG_COUNT;
    }

    static void PrintTrace() {
        std::vector<std::pair<int, Operation>> ops;
        for (int threadNumber = 0; threadNumber < storage.containers.size(); threadNumber++)
            for (const auto threadLocalOperation : *storage.containers[threadNumber].second)
                ops.push_back(std::make_pair(threadNumber, threadLocalOperation));

        std::sort(ops.begin(), ops.end(), [](const auto& a, const auto& b){
            return a.second.time < b.second.time;
        });

        for (int i = 0; i < ops.size(); i++) {
            printf("%d / %u:          ", i, int(ops[i].second.time));
            for (int j = 0; j < ops[i].first * 25; j++)
                printf(" ");
            printf("%d %020zx  ", int(ops[i].second.type), ops[i].second.address);
            printf("\n");
        }
        fflush(stdout);
    }

private:
    FastLogger() {
        std::lock_guard guard{storage.lock};
        storage.containers.push_back({std::this_thread::get_id(), &data});
        data.resize(MAX_LOG_COUNT);
        currentIndex = 0;
    }

    static inline Storage storage;
    std::vector<Operation> data;
    int currentIndex;
};

} // namespace LFStructs

#endif // FAST_LOGGING_ENABLED
