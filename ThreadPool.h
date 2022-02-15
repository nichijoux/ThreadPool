#ifndef _ZH_THREADPOOL_H_
#define _ZH_THREADPOOL_H_

#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <functional>
#include <future>
#include <stdexcept>

//This is a threadPool write based on others who named Progschj
//and I just write some comments to learn how to write a threadPool by C++11
namespace zhThreadPool {
    class ThreadPool {
    private:
        //任务函数
        using Task = std::function<void()>;
        //线程池
        std::vector<std::thread> threadPool;
        //任务队列
        std::queue<Task> taskQueue;
        //互斥锁,数据同步
        std::mutex lock;
        //条件变量判断线程池任务
        std::condition_variable taskCondition;
        //线程池是否执行
        std::atomic<bool> run{true};
        //空闲线程数量
        std::atomic<int> idleThreadNumber{0};
    public:
        //构造函数
        inline explicit ThreadPool(size_t size = 8) {
            assert(size != 0);
            addThread(size);
        }

        //析构函数
        inline ~ThreadPool() {
            stopThreadPool();
        }

        //获取空闲线程数量
        inline int getIdleThreadNumber() const {
            return idleThreadNumber;
        }

        //获取线程池容量
        inline size_t getThreadPoolSize() const {
            return threadPool.size();
        }

        //提交任务(异步执行)
        //不能分开写,否则会出现undefined reference to 'decltype ({parm#1}*{parm#2})'错误
        template <typename Function, typename... Args>
        auto submit(Function &&function, Args &&... args) -> std::future<decltype(function(args...))> {
            if (!run) throw std::runtime_error("the threadPool is stopped");
            //否则则将任务加入准备队列
            //decltype不会
            using returnType = decltype(function(args...));
            //异步执行,packaged_task用于将函数打包并异步执行,又因为bind将函数与参数进行了绑定
            //因此模板参数只需要为returnType(),括号中不需要传递函数
            auto task = std::make_shared<std::packaged_task<returnType()>>(
                    std::bind(std::forward<Function>(function), std::forward<Args>(args)...));
            //获取future对象
            std::future<returnType> returnFuture = task->get_future();
            //将任务加入任务队列,将锁放入代码块中,可以使lockGuard快速解锁
            {
                std::lock_guard<std::mutex> lockGuard(this->lock);
                //taskQueue接受的是void()型函数,而打包的task对象有返回值returnType(),因此还需要封装
                //使用shared_ptr的task是因为在lambda表达式中使用&会导致future_error
                //而shared_ptr则不会导致堆内存错误
                taskQueue.emplace([task]() {
                    (*task)();
                });
            }
            //由于有新的任务入队,因此需要用条件变量进行唤醒
            taskCondition.notify_one();
            //异步执行获取返回值,不返回returnType类型,否则
            return returnFuture;
        }

        //添加线程
        void addThread(size_t size);

        //结束threadPool
        void stopThreadPool();
    };
}

#endif //_ZH_THREADPOOL_H_
