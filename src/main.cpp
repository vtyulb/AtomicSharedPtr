#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <mutex>
#include <stack>
#include <vector>
#include <queue>
#include <utility>
#include <signal.h>

#include "lfstack.h"
#include "lfqueue.h"

void check(bool good) {
    if (!good)
        abort();
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


template<typename T>
void stress_test_lockable_stack(int actionNumber, int threadCount) {
    std::vector<std::thread> threads;
    T container;
//    std::atomic_flag lock = ATOMIC_FLAG_INIT;
    std::mutex lock;
    for (int i = 0; i < threadCount; i++)
        threads.push_back(std::thread([i, actionNumber, &container, &lock, threadCount](){
            for (int j = 0; j < actionNumber / threadCount; j++) {
                bool op = rand() % 2;
//                while (lock.test_and_set()) {}
                lock.lock();
                if (op)
                    container.push(rand());
                else if (container.size())
                    container.pop();
//                lock.clear();
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
    for (int i = 500000; i <= 3000000; i += 500000) {
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

void all_queue_tests() {
    printf("running simple LFQueue test...\n");
    simple_queue_test();
    printf("\nrunning lf queue stress test...\n");
    abstractStressTest(stress_test<LFStructs::LFQueue<int>>);
    printf("\nrunning lockable queue stress test...\n");
    abstractStressTest(stress_test_lockable_stack<std::queue<int>>);
}

void all_stack_tests() {
    printf("\nrunning simple LFStack test...\n");
    printf("\nrunning lf stack stress test...\n");
    abstractStressTest(stress_test<LFStructs::LFStack<int>>);
    printf("\nrunning lockable stack stress test...\n");
    abstractStressTest(stress_test_lockable_stack<std::stack<int>>);
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
    all_queue_tests();
    all_stack_tests();
    return 0;
}
