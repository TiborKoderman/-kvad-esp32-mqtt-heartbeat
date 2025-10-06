#pragma once
#include <stdint.h>

struct FsmEvent { uint16_t id; const void* payload; };

template<class Ctx>
struct StateDesc {
  const char* name;
  void (*onEnter)(Ctx*);
  void (*onExit)(Ctx*);
  void (*onTick)(Ctx*, uint32_t);
  void (*onEvent)(Ctx*, const FsmEvent&);
  const StateDesc* (*next)(Ctx*);
};

template<class Ctx>
class Fsm {
public:
  Fsm(Ctx* ctx, const StateDesc<Ctx>* init) : ctx_(ctx), cur_(init) {
    if (cur_->onEnter) cur_->onEnter(ctx_);
  }
  void tick(uint32_t now){
    if (cur_->onTick) cur_->onTick(ctx_, now);
    auto* nx = cur_->next ? cur_->next(ctx_) : cur_;
    if (nx != cur_) { if (cur_->onExit) cur_->onExit(ctx_); cur_ = nx; if (cur_->onEnter) cur_->onEnter(ctx_); }
  }
  void dispatch(uint16_t id, const void* p=nullptr){
    if (cur_->onEvent) cur_->onEvent(ctx_, {id,p});
  }
  const StateDesc<Ctx>* current() const { return cur_; }
  const char* name() const { return cur_->name; }
private:
  Ctx* ctx_;
  const StateDesc<Ctx>* cur_;
};
