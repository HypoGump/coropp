#ifndef _COROPP_H_
#define _COROPP_H_

#include <functional>

namespace coro
{

using CoroutineFunc = std::function<void()>;
/*
 * co_XX are coroutine primitives.
 * They are thread local which means that you can't resume 
 * a coroutine in anthoer thread.
 */

/* create new coroutine */
int co_new(const CoroutineFunc& cb);

/* switch to main context */
void co_yield();

/* swaitch to coroutine context */
void co_resume(int coroId);

/* determine coroutine alive or not */
bool co_alive(int coroId);

/* get id of the running id */
int co_running_id();

} /* namespace coro */ 

#endif