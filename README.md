# coropp

基于 C++ 11 的 **有栈协程** 库，利用 thread_local / closure 等新特性隐藏掉了 Schedule，简化为只包含 5 个基本接口（作为云风大大的 [coroutine]( https://github.com/cloudwu/coroutine) 的学习总结）。用户只需要控制协程的 handle （协程ID）即可。

```c++
// 创建一个新协程，CoroutineFunc 即 std::function<void()>
int co_new(const CoroutineFunc& cb);
// 让出控制权
void co_yield();
// 恢复ID为 coroId 的协程
void co_resume(int coroId);
// ID为 coroId 的协程是否运行完毕
bool co_alive(int coroId);
// 获取正在运行的协程ID
int co_running_id();
```

## 使用样例

```c++
#include <stdio.h>
#include "coropp.h"

using namespace coro;

void foo(int base)
{
  for (int i = 0; i < 5; ++i) {
    printf("coroutine %d: %d\n", co_running_id(), base + i);
    co_yield();
  }
}

int main()
{
  int co1 = co_new([]{ foo(0); });
  int co2 = co_new([]{ foo(100); });

  printf("main start\n");
  while (co_alive(co1) && co_alive(co2)) {
    co_resume(co1);
    co_resume(co2);
  }
  printf("main end\n");

  return 0;
}
```

## 性能测试

环境 i5-8265U / RAM 8G / WSL-Ubuntu-18.04 LTS，单线程 2 个协程，各切换 一百万 次（见 [benchtest.cc](./benchtest.cc) ）

total time = **62.502766 s** ,  time per switch = **31251 ns** ，

# 协程

协程是非抢占式多任务调度的 subroutine 的推广。（拥有上下文环境的 goto 也很形象hhh）

> subroutine 是一段执行特定任务的程序指令。在不同语言中它可能被称作 procedure、function、routine、method、subprogram，更通用术语是 callable unit

+ 与 subroutine 的区别
  + subroutine 只返回一次，在互相调用之间不维护状态（只是上下文继续延展）
  + coroutine 则是上下文的切换，当调用另一个 coroutine 时，当前 coroutine 实际是退出的
+ 与 threads 的区别
  + 线程是 **抢占式** 的多任务调度
  + 协程是 **合作式** 的多任务调度。协程也可以用抢占式调度的线程来实现，但是会失去对硬实时操作的适用性和较小切换开销的优势
+ 与 generator 的区别
  + generator 也被称作 semicoroutine，属于 coroutine 的一种。
  + coroutine 能够控制 yield 之后继续执行的的位置；而 generator 则是将控制权交还给了 generator 的调用者，即 generator 不是指明了跳转位置，而是将一个值传给了 parent routine
+ 与 互递归 的区别 
  + 当使用 coroutine 实现状态机或者并发时，其行为与 尾调用的互递归 类似，但是 coroutine 更加灵活且高效
  + coroutine 可以维护状态（包括变量、执行点，可以不必每次从头执行），且 yield 位置不局限于尾部；互递归必须通过共享变量或者传参的形式接续执行，且每次调用会产生新的 stack frame

## 有栈/无栈协程

+ 无栈协程比有栈协程效率更高
+ 有栈协程可以从任意位置切出和恢复（会保存上下文信息，然后进行上下文切换），而无栈协程只能 suspend 栈顶的帧（只能在同级调用中挂起/恢复，不能通过嵌套函数挂起/恢复）

> 每一次函数调用都会在调用栈上维护一个独立的栈帧，一般包括
>
> 1. 函数的返回地址和参数
> 2. 临时变量
> 3. 函数调用的上下文，即栈帧的范围。寄存器 ebp 指向栈帧底（帧指针），esp 指向栈帧顶（栈指针）

```python
# python 的 generator 是非对称无栈协程
# 可以在同级中挂起和恢复
def gfrange(fromn, endn, stepn):
    while fromn < endn:
        yield fromn
        fromn += stepn

# 不能通过嵌套函数挂起/恢复
def square(n):
    yield n**2
    
def gfrange(fromn, endn, stepn):
    while fromn < endn:
        yield square(fromn)
        fromn += stepn
```

## 对称/非对称协程

+ 对称协程的控制权在协程之间传递，协程每次挂起需要指定一个明确的切换目标（只有一种程序控制权传递操作 transfer）
+ 非对称协程挂起时控制权返回给调用者（两种程序控制权传递操作 resume、yield）

