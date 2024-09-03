#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    // 构造函数
    ThreadPool(size_t);
    // 添加任务函数
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    // 析构函数
    ~ThreadPool();

private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;         // 线程数组
    // the task queue
    std::queue< std::function<void()> > tasks;  // 任务队列
    
    // synchronization
    std::mutex queue_mutex;             // 互斥量
    std::condition_variable condition;  // 条件变量
    bool stop;                          // 停止标志位
};
 
// the constructor just launches some amount of workers(stop = false)
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        // 依次调用线程处理
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    std::function<void()> task;

                    {
                        // 对互斥量加锁
                        std::unique_lock<std::mutex> lock(this->queue_mutex);

                        // 等待lambda函数返回值产生
                        // 如果没有任务 / stop了，则阻塞，互斥量解锁 => 释放给其他线程
                        // 有任务 / 没有stop，则不阻塞线程，互斥量加锁 => 处理任务
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });
                        // 没有处理任务=>直接返回
                        if(this->stop && this->tasks.empty())
                            return;
                        // 从任务队列中取出任务
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task(); // 处理任务

                    // unique_lock自动解锁
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    // 指定“返回值类型”别名
    using return_type = typename std::result_of<F(Args...)>::type;

    // 将bind()函数包装起来并分配其智能指针给task变量
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
    
    // 获取task的future对象
    std::future<return_type> res = task->get_future();
    {
        // 加锁
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        // 如果停止使用线程池，则不可以添加任务；
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        // 添加任务
        tasks.emplace([task](){ (*task)(); });

        // 互斥量自动解锁
    }

    // 把wait()线程唤醒=>解除阻塞到condition条件变量的线程
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // 停止使用线程池
        stop = true;
    }
    // 唤醒所有wait()
    condition.notify_all();

    // 所有线程汇合
    for(std::thread &worker: workers)
        worker.join();
}

#endif
