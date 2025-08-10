# C++11特性介绍

本节我们将介绍在MyMuduo网络库中使用到的主要C++11特性。

## 1. 智能指针
- `std::shared_ptr`：共享所有权的智能指针
- `std::weak_ptr`：弱引用，配合shared_ptr使用
- `std::unique_ptr`：独占所有权的智能指针

```cpp
std::shared_ptr<Connection> conn(new Connection());
std::weak_ptr<Connection> weakConn = conn;
```

## 2. 右值引用和移动语义
- 使用`&&`表示右值引用
- `std::move()`将左值转换为右值
- 移动构造函数和移动赋值运算符

```cpp
class Buffer {
    Buffer(Buffer&& other) noexcept;  // 移动构造函数
    Buffer& operator=(Buffer&& other) noexcept;  // 移动赋值运算符
};
```

## 3. Lambda表达式
- 用于创建匿名函数对象
- 常用于回调函数

```cpp
auto callback = [](const TcpConnectionPtr& conn) {
    // 处理连接
};
```

## 4. auto关键字
- 自动类型推导
- 简化代码，提高可读性

```cpp
auto result = calculateResult();
auto it = container.begin();
```

## 5. nullptr
- 替代NULL的空指针常量
- 类型安全

```cpp
void* ptr = nullptr;
```

## 6. 范围for循环
- 简化容器遍历

```cpp
for (const auto& item : container) {
    // 处理item
}
```

## 7. 线程支持
- `std::thread`：线程类
- `std::mutex`：互斥量
- `std::condition_variable`：条件变量
- `std::lock_guard`和`std::unique_lock`：RAII锁管理

```cpp
std::mutex mtx;
std::lock_guard<std::mutex> lock(mtx);
```

## 8. 原子操作
- `std::atomic`：原子类型
- 无锁编程支持

```cpp
std::atomic<int> counter{0};
counter++;
```

## 9. 时间工具
- `std::chrono`：时间库
- 高精度时间点和时间段

```cpp
using namespace std::chrono;
auto now = system_clock::now();
```

## 10. 函数对象包装器
- `std::function`：通用函数包装器
- `std::bind`：函数适配器

```cpp
std::function<void(const TcpConnectionPtr&)> callback;
```

这些C++11特性在MyMuduo中的应用：
1. 智能指针用于管理TCP连接和定时器等资源
2. 移动语义用于优化Buffer的数据传输
3. Lambda表达式用于注册回调函数
4. 原子操作用于计数器和标志位
5. 线程支持用于实现EventLoop和ThreadPool 