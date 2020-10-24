#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <mutex>
#include <stack>
#include <vector>
#include <queue>
#include <map>
#include <csignal>

#include "lfstack.h"
#include "lfqueue.h"
#include "lfmap.h"
#include "lfmap_avl.h"

void check(bool good) {
    if (!good)
        abort();
}

void atomic_shared_ptr_concurrent_store_load_test() {
    printf("running AtomicSharedPtr load/store test...\n");
    const auto threadCount = std::thread::hardware_concurrency();
    LFStructs::AtomicSharedPtr<int> sp(new int(0));
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < threadCount/2; i++) {
        threads.emplace_back([&sp]{
            for (int j = 0; j < 1000000; j++)
                sp.store(new int(42));
        });
    }
    for (unsigned i = threadCount/2; i < threadCount; i++) {
        threads.emplace_back([&sp]{
            for (int j = 0; j < 1000000; j++)
                sp.get();
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
}

void simple_stack_test() {
    LFStructs::LFStack<int> stack;
    stack.push(5);
    stack.push(6);
    stack.push(7);
    check(*stack.pop() == 7);
    check(*stack.pop() == 6);
    check(*stack.pop() == 5);
    check(!bool(stack.pop()));
    check(!bool(stack.pop()));
}

void simple_queue_test() {
    LFStructs::LFQueue<int> queue;
    queue.push(5);
    queue.push(6);
    queue.push(7);
    check(*queue.pop() == 5);
    check(*queue.pop() == 6);
    queue.push(8);
    check(*queue.pop() == 7);
    check(*queue.pop() == 8);
    queue.push(9);
    check(*queue.pop() == 9);
    check(!bool(queue.pop()));
    check(!bool(queue.pop()));
}

template<typename Map>
void simple_map_test() {
    Map map;
    map.upsert(5, 100);
    check(*map.get(5) == 100);
    map.upsert(7, 101);
    check(*map.get(5) == 100);
    map.upsert(6, 99);
    check(*map.get(5) == 100);
    check(*map.get(6) == 99);
    check(*map.get(7) == 101);
    map.remove(7);
    check(*map.get(5) == 100);
    check(!bool(map.get(7)));
}

template<typename Map>
void correctness_map_test() {
    Map lfMap;
    std::map<int, int> map;
    for (int i = 0; i <= 1000000; i++) {
        if (i % 100000 == 0) {
            printf("%d%%  ", i / 10000);
            fflush(stdout);
        }
        if (rand() % 2) {
            int key = rand() % 100;
            auto value = lfMap.get(key);
            if (!bool(value)) {
                check(map.find(key) == map.end());
            } else {
                int mapVal = map[key];
                check(mapVal == *value);
            }
        } else if (rand() % 2) {
            int key = rand() % 100;
            int value = rand() % 100;
            map[key] = value;
            lfMap.upsert(key, value);
        } else {
            int key = rand() % 100;
            map.erase(key);
            lfMap.remove(key);
        }
    }

    printf("\n");
}

template<typename Map>
void lfmap_stress_test(int actionNumber, int threadCount) {
    std::vector<std::thread> threads;
    Map map;
    for (int i = 0; i < 10000; i++)
        map.upsert(rand() % 1000000, rand());
    for (int i = 0; i < threadCount; i++)
        threads.push_back(std::thread([&map, actionNumber, threadCount](){
            const int MAX = 1000;
            for (int j = 0; j < actionNumber / threadCount; j++) {
                int op = rand() % 100;
                if (op < 1)
                    map.remove(rand() % MAX);
                else if (op < 2)
                    map.upsert(rand() % MAX, rand());
                else
                    map.get(rand() % MAX);
            }
        }));

    for (auto &thread : threads)
        thread.join();
}

void lockable_map_stress_test(int actionNumber, int threadCount) {
    std::vector<std::thread> threads;
    std::map<int, int> map;
    std::mutex mutex;
    for (int i = 0; i < 10000; i++)
        map[rand() % 1000000] = rand();
    for (int i = 0; i < threadCount; i++)
        threads.push_back(std::thread([&map, &mutex, actionNumber, threadCount](){
            const int MAX = 1000;
            for (int j = 0; j < actionNumber / threadCount; j++) {
                int op = rand() % 100;
                mutex.lock();
                if (op < 1)
                    map.find(rand() % MAX);
                else if (op < 2)
                    map[rand() % MAX] = rand();
                else
                    map.find(rand() % MAX);
                mutex.unlock();
            }
        }));

    for (auto &thread : threads)
        thread.join();
}

template<typename T>
void stress_test_lockable_stack(int actionNumber, int threadCount) {
    std::vector<std::thread> threads;
    T container;
    std::mutex lock;
    for (int i = 0; i < threadCount; i++)
        threads.push_back(std::thread([i, actionNumber, &container, &lock, threadCount](){
            for (int j = 0; j < actionNumber / threadCount; j++) {
                bool op = rand() % 2;
                lock.lock();
                if (op)
                    container.push(rand());
                else if (container.size())
                    container.pop();
                lock.unlock();
            }
        }));

    for (auto &thread : threads)
        thread.join();
}

template<typename T>
void stress_test(int actionNumber, int threadCount) {
    std::vector<std::thread> threads;
    std::vector<std::vector<int>> generated(threadCount);
    std::vector<std::vector<int>> extracted(threadCount);
    T container;
    std::atomic<int> initCount;
    for (int i = 0; i < threadCount; i++)
        threads.push_back(std::thread([i, actionNumber, &container, &generated, &extracted, &initCount, threadCount](){
            for (int j = 0; j < actionNumber / threadCount; j++) {
                if (rand() % 2) {
                    int a = rand();
                    container.push(a);
                    generated[i].push_back(a);
                } else {
                    auto a = container.pop();
                    if (a)
                        extracted[i].push_back(*a);
                }
            }
        }));

    for (auto &thread : threads)
        thread.join();

    std::vector<int> allGenerated;
    std::vector<int> allExtracted;
    for (int i = 0; i < generated.size(); i++)
        for (int j = 0; j < generated[i].size(); j++)
            allGenerated.push_back(generated[i][j]);

    for (int i = 0; i < extracted.size(); i++)
        for (int j = 0; j < extracted[i].size(); j++)
            allExtracted.push_back(extracted[i][j]);

    while (true) {
        auto a = container.pop();
        if (a)
            allExtracted.push_back(*a);
        else
            break;
    }

    check(allGenerated.size() == allExtracted.size());

    std::sort(allGenerated.begin(), allGenerated.end());
    std::sort(allExtracted.begin(), allExtracted.end());
    for (int i = 0; i < allExtracted.size(); i++)
        check(allGenerated[i] == allExtracted[i]);
}

void abstractStressTest(std::function<void(int, int)> f) {
    for (int i = 1; i <= std::thread::hardware_concurrency(); i++)
        printf("\t%d", i);
    printf("\n");
    for (int i = 500000; i <= 2000000; i += 500000) {
        printf("%d\t", i);
        for (int j = 1; j <= std::thread::hardware_concurrency(); j++) {
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            f(i, j);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            printf("%ld\t", std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
            fflush(stdout);
        }
        printf("\n");
    }
}

void all_map_tests() {
    printf("running simple LFMap test...\n");
    simple_map_test<LFStructs::LFMap<int, int>>();
    printf("running simple LFMapAvl test...\n");
    simple_map_test<LFStructs::LFMapAvl<int, int>>();

#ifndef MSAN
    printf("\nrunning correctness LFMap test...\n");
    correctness_map_test<LFStructs::LFMap<int, int>>();
    printf("\nrunning correctness LFMapAvl test...\n");
    correctness_map_test<LFStructs::LFMapAvl<int, int>>();
#endif

    printf("\nrunning LFMap stress test...\n");
    abstractStressTest(lfmap_stress_test<LFStructs::LFMap<int, int>>);
    printf("\nrunning LFMapAvl stress test...\n");
    abstractStressTest(lfmap_stress_test<LFStructs::LFMapAvl<int, int>>);

#ifndef MSAN
    printf("\nrunning lockable map stress test\n");
    abstractStressTest(lockable_map_stress_test);
#endif

    printf("\n\n");
}

void all_queue_tests() {
    printf("running simple LFQueue test...\n");
    simple_queue_test();
    printf("\nrunning LFQueue stress test...\n");
    abstractStressTest(stress_test<LFStructs::LFQueue<int>>);
    printf("\nrunning lockable queue stress test...\n");
    abstractStressTest(stress_test_lockable_stack<std::queue<int>>);
    printf("\n");
}

void all_stack_tests() {
    printf("running simple LFStack test...\n");
    simple_stack_test();
    printf("\nrunning LFStack stress test...\n");
    abstractStressTest(stress_test<LFStructs::LFStack<int>>);
    printf("\nrunning lockable stack stress test...\n");
    abstractStressTest(stress_test_lockable_stack<std::stack<int>>);
    printf("\n");
}

void abortTraceLogger(int sig) {
#if FAST_LOGGING_ENABLED
    LFStructs::FastLogger::PrintTrace();
    exit(0);
#endif
}

int main()
{
    signal(SIGABRT, abortTraceLogger);
    atomic_shared_ptr_concurrent_store_load_test();
    all_map_tests();
    all_queue_tests();
    all_stack_tests();
    return 0;
}
