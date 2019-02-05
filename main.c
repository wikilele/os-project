#include "utility.h"

#define QPAGE FRAME_SIZE
#define RAMTOP *((unsigned int *)0x2D4)


void schedule(void){
	struct tcb_t* tcb = thread_qhead(&running);
	if (tcb != NULL){
		thread_dequeue(&running);
		thread_enqueue(tcb,&readyQ);
	}
	if (list_empty(&readyQ)){
		/*system shut down*/
		if(thread_count == 1 && thread_qhead(&waitingQ) == SSI ) HALT(); /*|| thread_qhead(&running) == SSI)*/
		/*deadlock detection*/
		else if (thread_count > 0 && soft_block_count == 0) panic_msg("deadlock detected\n");
		/*waiting interrupts*/
		else {
			/*acking the previous time slice*/
			setTIMER(TIMESLICE);
			setSTATUS(STATUS_ALL_INT_ENABLE(getSTATUS())); 
			WAIT();
		}
	} else {
		tcb = thread_dequeue(&readyQ);
		thread_enqueue(tcb,&running);
		
		/*interval timer */
		setTIMER(TIMESLICE);
		LDST(&(tcb->t_s));
	}
}


int main(int argc, char* * argv[]) {
	state_t tmpstate; // temp processor state
	memaddr stackalloc = RAMTOP; 
	state_t* newarea;

	struct tcb_t *tcb;

	int i;
	int j;
	
	/*setting state_t for exeception handlers*/
	STST(&tmpstate);
	tmpstate.sp = stackalloc;
	tmpstate.cpsr = STATUS_ALL_INT_DISABLE(tmpstate.cpsr) & STATUS_CLEAR_MODE | STATUS_SYS_MODE;
	tmpstate.CP15_Control = CP15_DISABLE_VM(tmpstate.CP15_Control);

	/*setting exception handlers*/
	tmpstate.pc = (memaddr) SysBpExceptionHandler;
		newarea = (state_t*)SYSBK_NEWAREA;
		*newarea = tmpstate;
		
	tmpstate.pc = (memaddr) ProgTrapExceptionHandler;
		newarea = (state_t*)PGMTRAP_NEWAREA;
		*newarea = tmpstate;
		
	tmpstate.pc = (memaddr) TLBTrapExceptionHandler;
		newarea = (state_t*)TLB_NEWAREA ;
		*newarea = tmpstate;
		
	tmpstate.pc = (memaddr) InterruptExceptionHandler;
		newarea = (state_t*)INT_NEWAREA ;
		*newarea = tmpstate;
		

	root = proc_init();
	/* initialize data structures */
	thread_init();
	msgq_init();
	/*initialize scheduling queues*/
	INIT_LIST_HEAD(&readyQ);
	INIT_LIST_HEAD(&running);
	INIT_LIST_HEAD(&waitingQ);

	/*variables for deadlock detection*/
	thread_count = 0;
	soft_block_count= 0;

	/*io_matrix initialization*/
	for( i = 0 ; i < DEV_USED_INTS ; i++){
		for(j = 0 ; j < DEV_PER_INT ; j ++){
			INIT_LIST_HEAD(&io_matrix[i][j]);
		}
	}

	/*setting SSI*/
	SSI = thread_alloc(root);
	tmpstate.sp = (stackalloc -= QPAGE);
	tmpstate.pc = (memaddr) SSI_function_entry_point;
	SSI->t_s = tmpstate;
	SSI->t_status = T_STATUS_READY;
	thread_enqueue(SSI, &readyQ);
	thread_count++;

	/*setting test function*/
	tcb = thread_alloc(proc_alloc(root));
	tmpstate.cpsr = STATUS_ALL_INT_ENABLE(tmpstate.cpsr);
	tmpstate.sp = (stackalloc -= QPAGE);
	tmpstate.pc = (memaddr) test;
	tcb->t_s = tmpstate;
	tcb->t_status = T_STATUS_READY;
	thread_enqueue(tcb, &readyQ);	
	thread_count++;
	
	schedule();
	
}