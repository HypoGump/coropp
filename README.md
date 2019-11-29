# coropp

基于 C++ 11 的协程库，利用 thread_local / closure 等新特性，简化为只包含 5 个基本接口（作为云风大大的 [coroutine]( https://github.com/cloudwu/coroutine) 的学习总结）

```c++
// 创建一个新协程
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



# 协程

