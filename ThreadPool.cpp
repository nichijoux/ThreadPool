#include "ThreadPool.h"

namespace zhThreadPool {
    void ThreadPool::addThread(size_t size) {
        for (size_t i = 0; i < size; ++i) {
            threadPool.emplace_back([this]() {
                while (true) {
                    Task task;
                    {
                        //条件变量的wait函数接收unique_lock参数
                        std::unique_lock<std::mutex> uniqueLock(lock);
                        //std::condition_variable::wait(unique_lock<mutex>& lck,Predicate pred)函数
                        //只有当 pred 条件为 false 时调用 wait() 才会阻塞当前线程
                        //并且在收到其他线程的通知后只有当 pred 为 true 时才会被解除阻塞
                        taskCondition.wait(uniqueLock, [this]() {
                            return !run || !taskQueue.empty();
                        });
                        //如果threadPool不运行了则直接return
                        if (!run && taskQueue.empty()) return;
                    }
                    //向任务队列中取任务
                    task = std::move(taskQueue.front());
                    taskQueue.pop();
                    //真正执行任务
                    idleThreadNumber--;
                    task();
                    idleThreadNumber++;
                }
            });
            idleThreadNumber++;
        }
    }

    //结束threadPool
    void ThreadPool::stopThreadPool() {
        run = false;
        //执行剩余线程,因为run == false,因此剩余线程应直接return
        taskCondition.notify_all();
        for (std::thread &thread: threadPool) {
            if (thread.joinable())
                thread.join();
        }
    }
}
