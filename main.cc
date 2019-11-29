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