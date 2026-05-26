#pragma once
#include "tcb.h"

/* Prototypes aligned with the real symbols defined in atomport_asm.s */
void swapContext(TCB *old_tcb_ptr, TCB *new_tcb_ptr);
void setContext(TCB *new_tcb_ptr);