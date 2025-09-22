i#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class SimpleThreadPool {
public:
    // 构造函数：创建并启动线程
    SimpleThreadPool(size_t numThreads);
    // 析构函数：停止线程池
    ~SimpleThreadPool();

    // 任务入队
    void enqueue(std::function<void()> task);

private:
    // 线程容器
    std::vector<std::thread> workers;
    // 任务队列
    std::queue<std::function<void()>> tasks;

    // 同步原语
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;
};

// 构造函数实现
SimpleThreadPool::SimpleThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    // 锁住临界区
                    std::unique_lock<std::mutex> lock(this->queue_mutex);

                    // 等待直到有任务或线程池停止
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });

                    // 如果被唤醒是因为要停止，且任务队列已空，则退出循环
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }

                    // 取出任务
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                } // 锁在此处释放

                // 执行任务
                task();
            }
        });
    }
}

// 析构函数实现
SimpleThreadPool::~SimpleThreadPool() {
    {
        // 锁住临界区，设置停止标志
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    // 唤醒所有等待的线程
    condition.notify_all();

    // 等待所有线程执行完毕
    for (std::thread &worker : workers) {
        worker.join();
    }
}

// 任务入队实现
void SimpleThreadPool::enqueue(std::function<void()> task) {
    {
        // 锁住临界区
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        // 如果线程池已停止，则不再添加任务
        if (stop) {
            // 可以选择抛出异常或直接返回
            return; 
        }

        // 将任务放入队列
        tasks.emplace(std::move(task));
    }
    // 唤醒一个线程来处理任务
    condition.notify_one();
}

// --- Main函数：使用示例 ---
void print_task(int i) {
    std::cout << "Task " << i << " is running on thread " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
    std::cout << "Creating a thread pool with 4 threads." << std::endl;
    SimpleThreadPool pool(4);

    // 提交10个任务
    for (int i = 0; i < 10; ++i) {
        // 使用 lambda 表达式来包装任务
        pool.enqueue([i] {
            print_task(i);
        });
    }

    std::cout << "All tasks submitted. Main thread will sleep for 3 seconds." << std::endl;
    
    // 等待一下，让线程池有时间处理任务
    // 在实际应用中，你会有其他同步机制来等待任务完成
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "Main thread exiting. ThreadPool destructor will be called." << std::endl;

    // 当 main 函数结束，pool 对象会被销毁，自动调用析构函数，优雅地关闭线程池
    return 0;
}
