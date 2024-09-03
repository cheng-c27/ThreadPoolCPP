#include <iostream>
#include <vector>
#include <chrono>
#include "ThreadPool.h"
int main()
{
    // 线程池大小为4
    ThreadPool pool(4);
    std::vector< std::future<int> > results;

    // 添加8个任务给线程池处理
    for(int i = 0; i < 8; ++i) {
        // 获取任务结果
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "world " << i << std::endl;
                return i*i;
            })
        );
    }

    // 打印任务处理结果
    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
    
    return 0;
}