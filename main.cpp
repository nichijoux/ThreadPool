#include <iostream>
#include <functional>
#include <future>
#include <queue>
#include "ThreadPool.h"

using namespace std;

//run function Like submit
template <typename Function, typename... Args>
auto run(Function &&func, Args &&... args) -> std::future<decltype(func(args...))> {
    using returnType = decltype(func(args...));
    using Task = function<void()>;
    auto task = packaged_task<returnType()>(
            bind(forward<Function>(func), forward<Args>(args)...));
    queue<Task> taskQueue;
    taskQueue.emplace([&]() {
        task();
    });
    function<void()> taskFunc = taskQueue.front();
    taskQueue.pop();
    taskFunc();
    return task.get_future();
}

int test(int a) {
    printf("this is a test function and arg is %d\n", a);
    return 0;
}

struct A {
private:
    int num{0};
public:
    void func1(int id) const {
        printf("this is a test fun1 and the num is %d and the id is %d\n", num, id);
    }

    static void func2(int id) {
        printf("this is a static fun2 and the num is %d\n", id);
    }
};

struct B{
    std::string operator()(){
        return "this is a operator()\n";
    }
};

int main() {
    cout << "hello world" << endl;
    try {
        zhThreadPool::ThreadPool pool(4);
        A a;
        auto ret1 = pool.submit(test, 13);
        auto ret2 = pool.submit(A::func2, 14);
        auto ret3 = pool.submit(bind(&A::func1,&a,1378));
        auto ret4 = pool.submit(B{});

        cout << ret4.get() << "\n";
    } catch (exception &e) {
        printf("%s\n", e.what());
    }
    return 0;
}