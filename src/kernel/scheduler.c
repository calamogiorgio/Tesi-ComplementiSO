#include <avr/interrupt.h>
#include <avr/io.h>
#include <assert.h>
#include "tcb.h"
#include "tcb_list.h"
#include "atomport_asm.h"
#include "timer.h"

// the (detached) running process
TCB* current_tcb=NULL;

// the running queue
TCBList running_queue={
  .first=NULL,
  .last=NULL,
  .size=0
};


void startSchedule(void){
  cli();
  current_tcb=TCBList_dequeue(&running_queue);
  assert(current_tcb);
  timerStart();
  setContext(current_tcb);
}

void schedule(void) {
  TCB* old_tcb=current_tcb;
  if (current_tcb->status != Waiting) {
    current_tcb->status = Ready;
    // we put back the current thread in the queue
    TCBList_enqueue(&running_queue, current_tcb);
}

  // we fetch the next;
  current_tcb=TCBList_dequeue(&running_queue);
  current_tcb->status = Running;
  // we jump to it (useless if it is the only process)
  if (old_tcb!=current_tcb)
    swapContext(old_tcb, current_tcb);
}
