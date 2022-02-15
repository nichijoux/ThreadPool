# ThreadPool based on Progschj

![language](https://img.shields.io/badge/language-c%2B%2B-orange)
![learning](https://img.shields.io/badge/category-learning-lightgrey)

learn form [Progschj's github](https://github.com/progschj/ThreadPool)

- [ThreadPool based on Progschj](#threadpool-based-on-progschj)
  - [核心思想](#核心思想)
    - [实现细节上](#实现细节上)
  - [std::thread](#stdthread)
    - [std::thread构造函数](#stdthread构造函数)
    - [move赋值操作](#move赋值操作)
    - [get_id()](#get_id)
    - [joinable()](#joinable)
    - [join()](#join)
    - [detach()](#detach)
  - [std::mutex](#stdmutex)
    - [(1)std::mutex](#1stdmutex)
    - [(2)std::recursive_mutex](#2stdrecursive_mutex)
    - [(3)std::timed_mutex](#3stdtimed_mutex)
    - [(4)std::recursive_timed_mutex](#4stdrecursive_timed_mutex)
  - [std::lock_guard](#stdlock_guard)
    - [std::lock_guard构造函数](#stdlock_guard构造函数)
  - [std::unique_lock](#stdunique_lock)
    - [std::unique_lock构造函数](#stdunique_lock构造函数)
  - [std::promise](#stdpromise)
  - [std::packaged_task](#stdpackaged_task)
    - [std::packaged_task构造函数](#stdpackaged_task构造函数)
    - [常用API](#常用api)
    - [和std::bind()结合](#和stdbind相结合)
  - [std::future](#stdfuture)
    - [std::future常用API](#stdfuture常用api)

## 核心思想

该线程池的核心思想是:
> 管理一个任务队列，一个线程队列，然后每次取一个任务分配给一个线程去做，循环往复  
> 特别之处在于,利用 `std::function<>` 实现了thread的重复利用

`ThreadPool`基于c++11，使用了各种C++11的特性,如
> `std::bind<>`  
> `std::function<>`  
> `std::packaged_task<>`  
> `std::make_shared<>`  
> `std::future<>`  
> `std::condition_variable<>`  
> `std::atomic<>`  
> `std::mutex`  
> `std::thread`

### 该线程池实现细节上

```cpp
1. //将返回值为void且不传递参数的函数包装成function对象,并将其命名为Task
using Task = std::function<void()>;
2. //使用模板泛化submit函数,并使用decltype自动推导返回值
template <class Function,class... Args>
auto submit(Function&& function,Args&& ...args)->decltype(function(args...));
3. //在submit函数内部
using returnType = decltype(function(args...));//获取返回类型
//std::bind()用于将函数与参数绑定,并通过std::forward完美转发
//绑定后返回一个function类型的对象,因此可以他通过std::package_task<>构造
//std::package_task<>此时的返回类型即为returnType而bind后不需要参数,因此为空即returnType()
//使用std::make_shared<>是因为task对象最终会被封装然后push到taskQueue中,因此需要使用堆内存
auto task = std::make_shared<std::package_task<returnType()>>(
        std::bind(std::forward<Function>(function),std::forward<Args>(args)...)
        );
4. //线程池mutex
{
    //自动取锁
    std::lock_guard<std::mutex> lockGuard(lock);
    //由于Task是void()类型的function,因此这里还需要对任务进行封装
    taskQueue.emplace([task](){
        //std::package_task<>重载了operator()
        (*task)();
    })
}
5. //其余线程使用
threadPool.emplace([this](){
    while(true){
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
    }
})
```

## std::thread

### std::thread构造函数

|construction|function|
|:---:|:---:|
|`default`(1)|`thread() noexcept`|
|`initialization`(2)|`template<class Fn,class...Args>explicit thread(Fn&& fn,Args&&... args);`|
|`copy(deleted)`(3)|`thread(const thread&)=delete;`|
|`move`(4)|`thread(thread&& x)noexcept`|

+ (1)默认构造函数，创建一个空的`thread`执行对象
+ (2)初始化构造函数，创建一个`thread`对象，该`thread`对象可被`joinable`，新产生的线程会调用`fn`函数，该函数的参数由`args`给出
+ (3)拷贝构造函数(`deleted`)，`thread`不可被拷贝构造
+ (4)移动构造函数，调用成功后x不代表任何`thread`对象
+ 可`joinable`的`thread`对象都必须在他们被销毁之前被`join`或`detached`

### move赋值操作

|move|function|
|:---:|:---:|
|`move`(1)|`thread& operator=(thread&& t)noexcept`|
|`copy(deleted`)(2)|`thread& operator=(const thread&) = delete`|

+ (1)`move`赋值操作，如果当前对象不可`joinable`，需传递一个右值引用(t)给`move`赋值操作；如果当前对象可`joinable`，则`terminate()`报错
+ (2)拷贝赋值操作被禁用，thread对象不可拷贝

### get_id()

获取线程ID
    
+ 如果当前对象 **`joinable`**,则返回唯一标识当前线程的值
+ 如果当前对象 **`non-joinable`**,则返回`thread::id`的默认构造对象

### joinable()

检查线程是否可被`join`
    
+ 如果`thread`表示一个执行线程，则其`joinable`  
+ 如果`thread`为默认构造，或该对象已经被移动，或它已经被`join()`或者`detach()`

### join()

阻塞当前线程直至 `thread` 所标识的线程结束其执行。
    
+ `thread` 所标识的线程的完成同步于对应的从 `join()` 成功返回。
+ `thread` 自身上不进行同步。同时从多个线程在同一 `thread` 对象上调用 `join()` 构成数据竞争，导致未定义行为。
+ 在调用此函数之后，线程对象将变为`non-joinable`，可以安全地`destroyed`。

### detach()

+ 从 `thread` 对象分离执行线程，允许执行独立地持续。一旦该线程退出，则释放任何分配的资源。
+ 调用 `detach()` 后 当前`thread` 不再占有任何线程。
+ 在调用此函数之后，线程对象将变为`non-joinable`，可以安全地`destroyed`。

## std::mutex

+ `std::mutex`，最基本的`mutex`
+ `std::recursive_mutex`，可递归`mutex`
+ `std::timed_mutex`，定时`mutex`
+ `std::recursive_timed_mutex`，定时可递归`mutex`

### (1)std::mutex

1. **`std::mutex`构造函数**

`std::mutex`是C++11中最基本的互斥量，`std::mutex`对象提供了独占所有权的特性——即不支持递归地对`std::mutex`对象上锁，而`std::recursive_lock`则可以递归地对互斥量对象上锁。

|construction|function|
|:---:|:---:|
|`default`(1)|`mutex() noexcept`|
|`copy`(2)|`mutex(const mutex&)=delete`|

+ (1) 构造互斥。构造后互斥在未锁定状态。
+ (2) 禁止拷贝构造函数

2. **`move`赋值操作**

`mutext& operator=(const mutex&)=delete`禁止拷贝构造

3. **`lock()`**

调用线程将锁住该互斥变量，线程调用会发生下面3种情况:

+ (1)如果该`mutex`未被锁住，则调用线程将锁住`mutex`，直到调用`unlock()`前都一直拥有该锁
+ (2)如果当前`mutex`被其他线程锁住，则当前调用线程将阻塞
+ (3)`lock()`已获取的`mutex`将导致未定义行为，可能导致死锁

4. **`unlock()`**

解锁，释放对互斥量的所有权

5. **`try_lock()`**

尝试锁住互斥量，如果互斥量被其他线程占有，当前线程也不会被阻塞。线程调用该函数也会出现下面 3 种情况，

+ (1)如果该`mutex`未被锁住，则调用线程将锁住`mutex`，直到调用`unlock()`前都一直拥有该锁
+ (2)如果当前`mutex`被其他线程锁住，且当前调用线程并不会被阻塞。
+ (3)`lock()`已获取的`mutex`将导致未定义行为，可能导致死锁

### (2)std::recursive_mutex

+ `std::recursive_mutex`与`std::mutex`一样，也是一种可以被上锁的对象，但是和`std::mutex`不同的是，`std::recursive_mutex`
  允许同一个线程对互斥量多次上锁（即递归上锁），来获得对互斥量对象的多层所有权
+ `std::recursive_mutex`释放互斥量时需要调用与该锁层次深度相同次数的`unlock()`，可理解为`lock()`次数和`unlock()`次数相同，除此之外`std::recursive_mutex`
  的特性和`std::mutex`大致相同

### (3)std::timed_mutex

`std::timed_mutex`比`std::mutex`多了两个成员函数，`try_lock_for()`和`try_lock_until()`

1. `try_lock_for(rel_time)`该函数接受一个时间范围
    1. 如果`mutex`未被锁住则调用线程将锁住`mutex`，直到调用`unlock()`前都一直拥有该锁；
    2. 如果`mutex`被其他线程锁住，则当前线程将在`rel_time`时间段内等待，如果在此期间`mutex`被其他线程释放，则当前线程将锁住`mutex`，否则将放弃获取`mutex`；
    3. `try_lock_for(rel_time)`已获取的`mutex`将导致未定义行为，可能导致死锁
    4. 若成功获得锁则为`true`，否则为`false`
2. `try_lock_until(timeout_time)`
    1. 如果`mutex`未被锁住则调用线程将锁住`mutex`，直到调用`unlock()`前都一直拥有该锁；
    2. 如果`mutex`被其他线程锁住，则当前线程将被阻塞直到`timeout_time`或者`mutex`被释放，如果在此期间`mutex`被其他线程释放，则当前线程将锁住`mutex`，否则将放弃获取`mutex`
       并返回`false`；
    3. `try_lock_until(timeout_time)`已获取的`mutex`将导致未定义行为，可能导致死锁
    4. 若成功获得锁则为`true`，否则为`false`

### (4)std::recursive_timed_mutex

和`std:recursive_mutex`与`std::mutex`的关系一样，`std::recursive_timed_mutex`的特性也可以从`std::timed_mutex`推导出来

## std::lock_guard

```cpp
template <class Mutex>
class lock_guard;
```

`lock_guard`用于管理某个`mutex`，它对`mutex`进行了封装，能方便线程对`mutex`上锁和解锁

模板参数`Mutex`代表互斥量类型，例如`std::mutex`类型为一个基本的`BasicLockable`类型，标准库中定义的几种`BasicLockable`类型即为`std::mutex`
、`std::recursive_mutex`、`std::timed_mutex`、`std::recursive_timed_mutex`、`std::unique_mutex`

+ `BasicLockable`类型只需满足`lock()`和`unlock()`操作
+ `Lockable`类型在`BasicLockable`类型上增加了`try_lock()`操作
+ `TimedLockable`类型则在`Lockable`类型上又新增了`try_lock_for(rel_time)`和`try_lock_until(timeout_time)`操作

`lock_guard`对象并不负责管理`mutex`对象的生命周期，`lock_guard`只是简化了`lock()`和`unlock()`操作

### std::lock_guard构造函数

|construction|function|
|:---:|:---:|
|`locking`(1)|`explicit lock_guard(mutex_type& m);`|
|`adopting`(2)|`lock_guard(mutex_type& m,adopt_lock_t tag);`|
|`copy`(`deleted`)(3)|`lock_guard(const lock_guard&)=delete;`|

1. **`locking`初始化**
   `lock_guard`对象管理`Mutex`对象`m`，并在构造时对`m`进行上锁
2. **`adopting`初始化**
   `lock_guard`对象管理`Mutex`对象`m`，但在构造之前`Mutex`类型的`m`就已经被当前线程锁住
3. **拷贝构造**
   `lock_guard`对象不可拷贝构造也不能移动构造

## std::unique_lock

相较于`std::lock_guard`，`std::unique_lock`提供了更好的`lock()`和`unlock()`操作，其余基本一致

### std::unique_lock构造函数

|construction|function|
|:---:|:---:|
|`default`(1)|`unique_lock() noexcept;`|
|`locking`(2)|`explicit unique_lock(mutex_type& m);`|
|`try-locking`(3)|`unique_lock(mutex_type& m,try_to_lock_t tag);`|
|`deferred`(4)|`unique_lock(mutex_type& m,defer_lock_t tag) noexcept;`|
|`adopting`(5)|`unique_lock(mutex_type& m,adopt_lock_t tag)`|
|`locking-for`(6)|`template<class Rep,class Period> unique_lock(mutex_type& m,const chrono::duration<Rep,Period>& rel_time);`|
|`locking-until`(7)|`template<class Clock,class Duration> unique_lock(mutex_type& m,const chrono::time_point<Clock,Duration>& abs_time);`|
|`copy`(`deleted`)(8)|`unique_lock(const unique_lock&)=delete`|
|`move`(9)|`unlock(unique_lock&& x);`|

1. 默认构造函数，新创建的`unique_lock`对象不管理任何`Mutex`对象。
2. `locking`初始化，新创建的`unique_lock`对象管理`Mutex`对象`m`，并尝试调用`lock()`对`Mutex`对象进行上锁，如果此时另外某个`unique_lock`对象已经管理了该`Mutex`
   ，则当前线程将会被阻塞。
3. `try-locking`初始化，新创建的`unique_lock`对象管理`Mutex`对象`m`，并尝试调用`try_lock()`对`Mutex`对象进行上锁，但如果上锁不成功，并不会阻塞当前线程。
4. `deferred`初始化，新创建的`unique_lock`对象管理`Mutex`对象`m`，但是在初始化的时候并不锁住`Mutex`对象。`m`应该是一个没有当前线程锁住的`Mutex`对象。
5. `adopting`初始化，新创建的`unique_lock`对象管理`Mutex`对象`m`，`m`应该是一个已经被当前线程锁住的`Mutex`对象。(并且当前新创建的`unique_lock`对象拥有对锁(`Lock`)的所有权)
6. `locking`一段时间(`duration`)，新创建的`unique_lock`对象管理`Mutex`对象`m`，并试图通过调用`try_lock_for(rel_time)`来锁住`Mutex`
   对象一段时间(`rel_time`)
7. `locking`直到某个时间点(`time point`)，新创建的`unique_lock`对象管理`Mutex`对象`m`，并试图通过调用`try_lock_until(abs_time)`来在某个时间点(`abs_time`)
   之前锁住`Mutex`对象。
8. 拷贝构造，`unique_lock`对象不能被拷贝构造。
9. 移动(`move`)构造，新创建的`unique_lock`对象获得了由`x`所管理的`Mutex`对象的所有权(包括当前`Mutex`的状态)。调用`move`构造之后，`x`
   对象如同通过默认构造函数所创建的，就不再管理任何`Mutex`对象了。

## std::promise

> `promise`对象可以保存某一类型`T`的值，该值可被`std::future`对象读取（可能在另外一个线程中），因此`promise`也提供了一种线程同步的手段。在`promise`对象构造时可以和一个共享状态（通常是`std::future`）相关联，并可以在相关联的共享状态(`std::future`)上保存一个类型为`T`的值。  
> 可以通过`get_future`来获取与该`promise`对象相关联的`future`对象，调用该函数之后，两个对象共享相同的共享状态(`shared state`)  
> `promise`对象是异步`Provider`，它可以在某一时刻设置共享状态的值。  
> `future`对象可以异步返回共享状态的值，或者在必要的情况下阻塞调用者并等待共享状态标志变为`ready`，然后才能获取共享状态的值。

`std::promise`常用API有  
`get_future()`、`set_value()`、`set_exception()`

## std::packaged_task

`std::packaged_task`包装一个可调用的对象，且允许异步获取被包装程序产生的结果，`std::packaged_task`类似于`std::function`，但`std::packaged_task`
会将执行结果传递给共享的`std::future`对象，并通过`std::future`对象获取异步执行的结果

### std::packaged_task构造函数

|construction|function|
|:---:|:---:|
|`default`(1)|`packaged_task() noexcept;`|
|`initialization`(2)|`template <class Fn> explicit packaged_task(Fn&& fn);`|
|`with allocator`(3)|`template <class Fn,class Alloc> explicit packaged_task(allocator_arg_t aa,const Alloc& alloc,Fn&& fn);`|
|`copy`(`deleted`)(4)|`packaged_task(const packaged_task&) = delete;`|
|`move`(5)|`packaged_task(packaged_task&& x) noexcept;`|

1. 默认构造函数，初始化一个空的共享状态，并且该`packaged_task`对象无包装任务。
2. 初始化一个共享状态，并且被包装任务由参数`fn`指定。
3. 带自定义内存分配器的构造函数，与默认构造函数类似，但是使用自定义分配器来分配共享状态。
4. 拷贝构造函数，被禁用
5. 移动构造函数

### 常用API

1. **`valid()`**
   检查当前`packaged_task`是否和一个有效的共享状态相关联，对于由默认构造函数生成的`packaged_task`对象，该函数返回`false`，除非中间进行了`move`赋值操作或者`swap`操作
2. **`get_future()`**
   返回一个与`packaged_task`对象共享状态相关的`future`对象。返回的`future`对象可以获得由另外一个线程在该`packaged_task`对象的共享状态上设置的某个值或者异常
3. **`operator()(Args... args)`**
   调用该`packaged_task`对象所包装的对象(通常为函数指针，函数对象，`lambda`表达式等)，传入的参数为`args`. 调用该函数一般会发生两种情况
    1. 如果成功调用`packaged_task`所包装的对象，则返回值(如果被包装的对象有返回值的话)被保存在`packaged_task`的共享状态中。
    2. 如果调用`packaged_task`所包装的对象失败，并且抛出了异常，则异常也会被保存在`packaged_task`的共享状态中。
4. **`reset()`**
   重置`packaged_task`的共享状态，但是保留之前的被包装的任务

### 和std::bind()结合

`std::packaged_task`常与`std::bind()`相结合使用，如

```cpp
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
```

## std::future

`std::future`可以用来获取异步任务的结果，因此可以把它当成一种简单的线程间同步的手段  
`std::future`通常由某个`Provider`创建，`Provider`在某个线程中设置共享状态的值，与该共享状态相关联的`std::future`对象调用`get`(通常在另外一个线程中)
获取该值，如果共享状态的标志不为`ready`，则调用 `std::future::get`会阻塞当前的调用者，直到`Provider`设置了共享状态的值(此时共享状态的标志变为`ready`)，`std::future::get`
返回异步任务的值或异常。

一个有效(valid)的`std::future`对象通常由以下三种`Provider`创建，并和某个共享状态相关联。`Provider`可以是函数或者类

1. `std::async()`
2. `std::promise::get_future()`
3. `std::packaged_task::get_future()`

### std::future常用API

1. **`share()`**
   将返回一个`std::share_future`对象，在调用此函数之后，该`std::future`对象本身已经不和任何共享状态向关联即当前`std::future`变为`invalid`
2. **`get()`**
   当与当前`std::future`对象相关联的共享状态变为`ready`后，调用该函数将返回保存在共享状态中的值，如果共享状态的标志不为`ready`，则调用该函数将会阻塞当前的调用者，而此后一旦共享状态变为`ready`
   ，`get()`就将返回`Provider`所设置的共享状态的值或异常
3. **`wait()`**
   等待与当前`std::future`对象相关联的共享状态标志变为`ready`；如果共享状态标志 **不是** `ready`(此时`Provider`没有在共享状态上设置值或异常)
   ，则调用该函数将会阻塞当前线程，直到共享状态的标志变为`ready`；一旦共享状态的标志变为`ready`，`wait()`将解除阻塞，但`wait()`并不读取贡献状态的值或异常
4. **`wait_for(rel_time)`**
5. **`wait_until(point_time)`**
