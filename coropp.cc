#include "coropp.h"

#include <ucontext.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <iostream>

namespace coro
{

struct Coroutine;

/*
 * default params for schedule
 */
const int kDefaultCoroutineCapacity = 16;
const int kDefaultSharedCtxStackSize = 1024 * 1024; 

struct Schedule
{
  Schedule();
  ~Schedule();

  bool avail() const { return coroNums_ < capacity_; }
  int createCoroutine(const CoroutineFunc& cb);
  void yield();
  void resume(int coroId);
  void deleteCoroutine(int coroId);
  bool isCoroutineAlive(int coroId) const { return coros_[coroId] != nullptr; }

  char runtimeStack_[kDefaultSharedCtxStackSize];
  ucontext_t main_;
  int coroNums_;
  int capacity_;
  int runningCoroId_;
  std::vector<Coroutine*> coros_;
};


enum Status
{
  COROUTINE_READY,
  COROUTINE_RUNNING,
  COROUTINE_SUSPEND
};

struct Coroutine
{
  Coroutine(const CoroutineFunc& coroFunc);
  ~Coroutine();

  Schedule* ownerSchedule_;
  CoroutineFunc func_;
  ucontext_t ctx_;
  ptrdiff_t capacity_;
  ptrdiff_t size_;
  Status status_;
  char* runtimeStack_;
};


/* 
 * coroutines shoulde be scheduled in same thread where they are created,
 * and there is only one schedule per thread
 */
thread_local Schedule t_schedule;

/*
 * for compatible with C function pointer
 */
void mainfunc() 
{
  int id = t_schedule.runningCoroId_;
  Coroutine* co = t_schedule.coros_[id];
  co->func_();
  t_schedule.deleteCoroutine(id);
  t_schedule.runningCoroId_ = -1;
}

/*
 * According to memory layout:
 *  1. dummy is at stack-top
 *  2. S->runtimeStack_ + STACK_SIZE is runtime stack-bottom? (it is in heap?)
 */
void saveCoroutineStack(Coroutine* co)
{
  char dummy = 0;
  char* top = t_schedule.runtimeStack_ + kDefaultSharedCtxStackSize;
  /* 
   * 1. when the coroutine run for fist time, create coroutine stack 
   * 2. when the coroutine run more deeper then yield, re-alloc stack
   */
  if (co->capacity_ < top - &dummy) {
    ::free(co->runtimeStack_);
    co->capacity_ = top - &dummy;
    co->runtimeStack_ = (char*)::malloc(co->capacity_);
  }
  co->size_ = top - &dummy;
  ::memcpy(co->runtimeStack_, &dummy, co->size_);
}

} /* namespace coro */

using namespace coro;

/*****************************************************************/
/*                   Coroutine Primitives                        */
/*****************************************************************/
int coro::co_new(const CoroutineFunc& cb)
{
  return t_schedule.createCoroutine(cb);
}

void coro::co_resume(int coroId)
{
  t_schedule.resume(coroId);
}

void coro::co_yield()
{
  t_schedule.yield();
}

bool coro::co_alive(int coroId)
{
  return t_schedule.isCoroutineAlive(coroId);
}

int coro::co_running_id()
{
  return t_schedule.runningCoroId_;
}

/*****************************************************************/
/*                          Coroutine                            */
/*****************************************************************/

Coroutine::Coroutine(const CoroutineFunc& coroFunc)
  : ownerSchedule_(&t_schedule),
    func_(coroFunc),
    capacity_(0),
    size_(0),
    status_(COROUTINE_READY)
{
}

Coroutine::~Coroutine()
{
  ::free(runtimeStack_);
}



/*****************************************************************/
/*                          Schedule                             */
/*****************************************************************/
Schedule::Schedule()
  : coroNums_(0),
    capacity_(kDefaultCoroutineCapacity),
    runningCoroId_(-1),
    coros_(capacity_, nullptr)
{
}

Schedule::~Schedule()
{
  for (auto& co : coros_) {
    if (co) delete co;
  }
}

int Schedule::createCoroutine(const CoroutineFunc& cb)
{
  Coroutine* newCoro = new Coroutine(cb);
  int coroId;

  if (avail()) {
    for (int i = 0; i < capacity_; ++i) {
      /* 
       * if there exist coroNums coroutines, then index > coroNums are
       * more likely to be free(nullptr)
       */
      int id = (i + coroNums_) % capacity_;
      if (coros_[id] == nullptr) {
        coroId = id;
        coros_[id] = newCoro;
        break;
      }
    }
  }
  else {
    coros_.push_back(newCoro);
    coroId = static_cast<int>(coros_.size()) - 1;
    capacity_ = coros_.capacity();
  }

  coroNums_++;
  return coroId;
}

void Schedule::deleteCoroutine(int coroId)
{
  delete coros_[coroId];
  coros_[coroId] = nullptr;
  --coroNums_;
}


void Schedule::resume(int coroId)
{
  Coroutine* co = coros_[coroId];
  if (co == nullptr) return;

  Status status = co->status_;
  switch (status) {
    case COROUTINE_READY:
      /* coroutine run first time */
      /* initialize the structure pointed at the currently active context */
      getcontext(&co->ctx_);

      /* 
       * uc_stack: the stack used by this context
       * uc_link: pointer to context that will be resumed when this context returns
       */
      co->ctx_.uc_stack.ss_sp = runtimeStack_;
      co->ctx_.uc_stack.ss_size = kDefaultSharedCtxStackSize;
      co->ctx_.uc_link = &main_;

      co->status_ = COROUTINE_RUNNING;
      runningCoroId_ = coroId;

      /* modify context and run coroutine func */
      makecontext(&co->ctx_, (void (*)(void)) mainfunc, 0);

      /* 
       * switch to coroutine 
       * swapcontext saves the current context in the structure pointed by 1st arg
       *        then activates the context pointed by the 2nd arg
       */
      swapcontext(&main_, &co->ctx_);
      break;
    case COROUTINE_SUSPEND:
      ::memcpy(runtimeStack_ + kDefaultSharedCtxStackSize - co->size_, co->runtimeStack_, co->size_);
      runningCoroId_ = coroId;
      co->status_ = COROUTINE_RUNNING;
      /* switch to coroutine */
      swapcontext(&main_, &co->ctx_);
      break;
    default:
      /* nothing to do */
      std::cout << "unkown/error status type\n";
  }
}

void Schedule::yield()
{
  Coroutine* co = coros_[runningCoroId_];

  saveCoroutineStack(co);
  co->status_ = COROUTINE_SUSPEND;
  runningCoroId_ = -1;

  /* switch to main context */
  swapcontext(&co->ctx_, &main_);
}