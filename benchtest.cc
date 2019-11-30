#include "coropp.h"
#include <stdio.h>
#include <chrono>

using namespace coro;
using namespace std::chrono;

const int kBenchNum = 1000 * 1000;

void foo(int base)
{
  for (int i = 0; i < kBenchNum; ++i) {
    printf("coroutine %d: %d\n", co_running_id(), base + i);
    co_yield();
  }
}

int main()
{
  int co1 = co_new([]{ foo(0); });
  int co2 = co_new([]{ foo(100); });

  printf("main start\n");
  auto start = system_clock::now();
  while (co_alive(co1) && co_alive(co2)) {
    co_resume(co1);
    co_resume(co2);
  }
  auto end = system_clock::now();
  auto interval = duration_cast<nanoseconds>(end-start);
  double total_time = double(interval.count()) / (1000 * 1000 * 1000);
  int unit = interval.count() / (2 * 1000 * 1000);

  printf("total time = %f s, time per switch = %d ns\n", total_time, unit);
  printf("main end\n");

  return 0;
}