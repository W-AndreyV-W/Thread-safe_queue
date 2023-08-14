#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <vector>
#include <queue>
#include <functional>
#include <future>



template <typename T> class SafeQueue {

public:

    void push(T& function);

    T pop();

    std::condition_variable& get_messages() {

        return messages;
    }

    std::stop_source& get_stop() {

        return stop_working;
    }

private:

    std::queue<T> thread_queue;
    std::mutex block;
    std::condition_variable messages;
    std::stop_source stop_working;
};

template <typename T> void SafeQueue<T>::push(T& function) {

    std::lock_guard lb(block);
    thread_queue.push(std::move(function));

    messages.notify_one();
}

template <typename T> T SafeQueue<T>::pop() {

    T variant_queue;

    std::unique_lock ul(block);

    messages.wait(ul, [&]() {

        if (!thread_queue.empty() || stop_working.stop_requested()) {

            return true;
        }
        else {

            return false;
        }
        });

    if (!stop_working.stop_requested()) {

        variant_queue = std::move(thread_queue.front());
        thread_queue.pop();
    }

    return variant_queue;
}



template <typename T> class ThreadPool {

public:

    ThreadPool() {

        cores = std::thread::hardware_concurrency() - 2;

        std::jthread wg (&ThreadPool::working, this);
    }

    ~ThreadPool() {

        safequeue.get_stop().request_stop();
        std::this_thread::yield();
        safequeue.get_messages().notify_all();
    }

    void submit(T function) {

        safequeue.push(function);
    }

private:

    int cores = 0;
    SafeQueue<T> safequeue;
    std::vector<std::jthread> pool;
    std::stop_token stop_work = safequeue.get_stop().get_token();

    void work();

    void working();
};

template <typename T> void ThreadPool<T>::work() {

    while (!stop_work.stop_requested()) {

        auto function = std::move(safequeue.pop());

        if (!stop_work.stop_requested()) {

            function();
        }
    }
}

template <typename T> void ThreadPool<T>::working() {

    for (int i = 0; i < cores; i++) {

        pool.push_back(std::jthread(&ThreadPool::work, this));
    }
}



void function1(std::mutex& block_function) {

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    std::lock_guard bl_fun(block_function);
    std::cout << __FUNCTION__ << " ... " << std::this_thread::get_id() << std::endl;
}

void function2(std::mutex& block_function) {

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    std::lock_guard bl_fun(block_function);
    std::cout << __FUNCTION__ << " ... " << std::this_thread::get_id() << std::endl;
}



int main() {

    int number = 20;
    std::mutex block_function;

    ThreadPool<std::packaged_task<void()>> threadpool;

    for (int i = 0; i < number; i++) {

        std::packaged_task<void()> func11(std::bind(function1, std::ref(block_function)));
        threadpool.submit(std::move(func11));

        std::packaged_task<void()> func22(std::bind(function2, std::ref(block_function)));
        threadpool.submit(std::move(func22));

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}