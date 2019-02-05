#include "utility.h"

#define INTLINEBASADDR  0x00006FE0 //interrupt line address base
#define ILLOPCODERR 	2
#define PSEUDOCLOCK  	100000
#define PSCLOCKRANGE    6000
#define PSEUDOCLOCKTOP  (PSEUDOCLOCK + PSCLOCKRANGE)
#define READPENDING		128

static inline void wake_up(struct tcb_t * tcb ,struct tcb_t* dest){
	/*if the dest is waiting a msg from everybody or from the sender tcb*/
	if (dest->t_status == T_STATUS_W4MSG &&
		 (dest->t_wait4sender == NULL ||
		 	 dest->t_wait4sender == tcb)){
		thread_outqueue(dest);
		dest->t_status = T_STATUS_READY;
		if(dest == SSI) ssi_push(dest,&readyQ);
		else thread_enqueue(dest, &readyQ);
		/*only if tcb is the SSI*/
		if(tcb == SSI)soft_block_count --;		
	}
}

static inline void wait(struct tcb_t * tcb , struct tcb_t* src, const int repeat){
	thread_outqueue(tcb); 
	tcb->t_status = T_STATUS_W4MSG;
	tcb->t_wait4sender = src;
	/*making dest repating the last instr*/
	if(repeat) tcb->t_s.pc = tcb->t_s.pc - 0x4;
	thread_enqueue(tcb,&waitingQ);
	if(src == SSI)soft_block_count ++;
}

static inline void fulfil_timeslice(struct tcb_t* tcb , const unsigned int timer){
	/*tcb is going to use its time slice*/
	setTIMER(timer);
	LDST(&(tcb->t_s));
}

static inline int is_mgr(struct tcb_t* tcb, struct tcb_t* mgr){
	struct pcb_t* pproc = tcb->t_pcb;
	return (pproc->pgmmgr == mgr ||
				pproc->tlbmgr == mgr ||
					pproc->sysmgr == mgr);
}

static inline void pseudoclock_check(void){ 
	static passed;
	unsigned int time = getTODLO(); // micro seconds
	struct tcb_t* tcb;
	int leave; //bool

	if (time < PSEUDOCLOCK ) passed = 0;
	time = time - passed;
	if (time >= PSEUDOCLOCK &&
			time <= PSEUDOCLOCKTOP  ){ 
		
		passed = passed + PSEUDOCLOCK;
		leave = 0;
		while(!list_empty(&SSI->t_generic) && !leave){
			tcb = thread_generic_qhead(&SSI->t_generic);
			/*payload added but not taken*/
			if( msgq_add(SSI, tcb, time - PSEUDOCLOCK) >= 0){
				thread_generic_dequeue(&SSI->t_generic);
				wake_up(SSI,tcb);
			} else leave = 1;
		}		
	}
}

void SysBpExceptionHandler(void){
	uintptr_t r1; //first register
	state_t* oldarea;
	struct tcb_t* tcb ; //thread making the sys call
	unsigned int timer ; //remaining time

	timer = getTIMER(); 
	pseudoclock_check();

	oldarea = (state_t*) SYSBK_OLDAREA ;
	r1 = oldarea->a1;	
	tcb = thread_qhead(&running); 
	/*saving old status in tcb */ 
	tcb->t_s = *oldarea;
	/*cputime management*/
	tcb->cputime = tcb->cputime + ( TIMESLICE - timer); 
	
	/*if the thread was in user mode*/
	if ( (r1 == SYS_SEND ||
			r1 == SYS_RECV ) && 
				(oldarea->cpsr & 0x001F) == STATUS_USER_MODE){		
		state_t* newarea;

		tcb->errno = SYSCALLERR;
		/*generate illegal instruction error*/
		tcb->t_s.CP15_Cause = CAUSE_EXCCODE_SET(tcb->t_s.CP15_Cause,EXC_RESERVEDINSTR );
		oldarea = (state_t*)  PGMTRAP_OLDAREA;
		newarea = (state_t*)  PGMTRAP_NEWAREA;
		*oldarea = tcb->t_s;
		LDST(newarea);
	} else {
		switch(r1){
			case 0 : {
				 __terminate_thread(tcb);
				 schedule();
				 break;
				}
			case SYS_SEND: {
				struct tcb_t* dest;
				uintptr_t payload ;
				dest =(struct tcb_t*)(oldarea->a2);
				payload = (uintptr_t)(oldarea->a3);
				/*checking if dest is a terminated thread*/
				if(dest != NULL &&
						!list_empty(&dest->t_sched)){ // && dest->t_status = T_STATUS_NONE
					/*adding the msg to the dest queue*/
					if (is_mgr(dest,tcb) &&
							dest->t_wait4sender == tcb &&
								payload == (uintptr_t)NULL){	// manager does: msgsend(sender,NULL);
						wake_up(tcb,dest); 
						tcb->t_s.a1 = 0;
					} else if ( msgq_add(tcb, dest, payload) >= 0){ 	// normal case
						wake_up(tcb,dest);
						tcb->t_s.a1 = 0;
					} else {
						tcb->t_s.a1 =-1;
						tcb->errno = SENDERR;
					}
				} else {
					/*dropping the msg*/
					tcb->t_s.a1 =-1;
					tcb->errno = SENDERR;
				}

				fulfil_timeslice(tcb,timer);
				break;
				}
			case SYS_RECV:{
				struct tcb_t* src;
				uintptr_t* payload_ptr;
				uintptr_t temp;
				
				src =(struct tcb_t*)(oldarea->a2);
				payload_ptr = (uintptr_t*)(oldarea->a3);
				
				if(payload_ptr == NULL) payload_ptr = &temp;	
				if(msgq_get(&src, tcb, payload_ptr) < 0){	
					/*msg not found*/					
					/*checking if src is a terminated thread*/
					if (src != NULL &&
						list_empty(&src->t_sched)){ // && src->t_status = T_STATUS_NONE
						*payload_ptr = (uintptr_t)NULL; //not necessary
						tcb->t_s.a1 = (uintptr_t)NULL;
						tcb->errno = RECVERR;
				 		fulfil_timeslice(tcb,timer);
					} else {
						wait(tcb,src,1);
						schedule();
					}
				}else {
					/*msg found*/
					tcb->t_s.a1 = (uintptr_t)src;
					fulfil_timeslice(tcb,timer);
				}
				break;
				}
			default : { 
				struct pcb_t * pproc;
				struct tcb_t * mgr;

				tcb->errno = SYSCALLERR;
				/*pass up to managers if they were declared else termination*/ 
				pproc = tcb->t_pcb; //parent process
				mgr = NULL; //manager
				if(pproc->sysmgr != NULL) mgr = pproc->sysmgr;
				else if(pproc->pgmmgr != NULL) mgr = pproc->pgmmgr;
				else { 
					__terminate_thread(tcb);
					schedule();
				}
				if(msgq_add(tcb, mgr, (uintptr_t)&(tcb->t_s)) >= 0){
				wake_up(tcb,mgr);
				wait(tcb,mgr,0);
				} else __terminate_thread(tcb);
				schedule();
				break;
				}
		}		
	}
} 

void ProgTrapExceptionHandler(void){
	state_t* oldarea;
	unsigned int timer;
	struct tcb_t* tcb;
	struct pcb_t * pproc;

	timer = getTIMER();
	pseudoclock_check();

	oldarea = (state_t*)  PGMTRAP_OLDAREA ;
	
	tcb = thread_qhead(&running);
	tcb->t_s = *oldarea;
	/*cputime management*/
	tcb->cputime = tcb->cputime + (TIMESLICE - timer);

	pproc = tcb->t_pcb; //parent process
	
	if(pproc->pgmmgr == NULL) {
		__terminate_process(tcb);
		schedule();
	} else {
		struct tcb_t* mgr;
		
		mgr = pproc->pgmmgr;
		if(msgq_add(tcb, mgr, (uintptr_t)&(tcb->t_s)) >= 0){
			wake_up(tcb,mgr);
			wait(tcb,mgr,1);
		} else __terminate_process(tcb);
		schedule();
	}	
}

void TLBTrapExceptionHandler(void){
	state_t* oldarea;
	unsigned int timer;
	struct tcb_t* tcb;
	struct pcb_t* pproc;

	timer = getTIMER();
	pseudoclock_check();
	oldarea = (state_t*)  TLB_OLDAREA ;

	tcb = thread_qhead(&running);
	tcb->t_s = *oldarea;
	/*cputime management*/
	tcb->cputime = tcb->cputime + (TIMESLICE - timer);

	pproc = tcb->t_pcb; //parent process
	
	if(pproc->tlbmgr == NULL) {
		__terminate_process(tcb);
		schedule();
	} else {
		struct tcb_t* mgr;
		
		mgr = pproc->tlbmgr;
		if (msgq_add(tcb, mgr, (uintptr_t)&(tcb->t_s)) >= 0){
		wake_up(tcb,mgr);
		wait(tcb,mgr,1);
		} else __terminate_process(tcb);
		schedule();
	}
} 

void InterruptExceptionHandler(void){
	unsigned int timer;
	state_t* oldarea;
	int line_no;
	int device_no;
	memaddr* intLine_ptr; //interrupt line pointer
	memaddr devAddrBase ; //device addres base
	memaddr* devCommand; //device command field
	memaddr* devStatus; //device status field
	uintptr_t tmp; //general purpose variable
	struct tcb_t* tcb;
	struct tcb_t* w4io;

	timer = getTIMER();
	pseudoclock_check();
	oldarea = (state_t*) INT_OLDAREA ;
	
	tcb = thread_qhead(&running);
	if(tcb != NULL){
		/*if line_no == INT_TIMER tcb is absolutely not NULL*/
		tcb->t_s = *oldarea;
		tcb->t_s.pc = tcb->t_s.pc - 0x4;
		/*cputime management*/
		tcb->cputime = tcb->cputime + (TIMESLICE - timer);
	}
	
	line_no = 2 ;
	device_no = 0;

	while(!CAUSE_IP_GET(oldarea->CP15_Cause,line_no))line_no ++ ;
	
	if(line_no != INT_TIMER){
		/*calculating device no*/
		intLine_ptr = (memaddr*)(INTLINEBASADDR + (line_no-3)*0x4);
		tmp = *intLine_ptr & 0x00FF;	
		while(!(tmp & (1 << device_no)))device_no ++;
		devAddrBase = 0x00000040 + ((line_no - 3) * 0x80) + (device_no * 0x10);
	}

	switch(line_no){
		case INT_TIMER:				
			/*scheduler will ack the interrupt resetting the int timer*/
			schedule();
			break;
		case INT_DISK:
		case INT_TAPE:
		case INT_UNUSED: /*network*/
		case INT_PRINTER:
			devStatus = (memaddr*)(devAddrBase + 0x0); //receive status
			devCommand = (memaddr*)(devAddrBase + 0x4); // receive command
			break;
		case INT_TERMINAL:			
			devStatus = (memaddr*)(devAddrBase + 0x8); //transmit status
			/*checking if is a recive interrupt or transmit interrupt*/ 
			tmp = *devStatus & 0x00FF;
			if( tmp == ILLOPCODERR ||  //Illegal Operation Code Error
					tmp == DEV_TTRS_S_TRSMERR || //Transmission Error
						tmp ==  DEV_TTRS_S_CHARTRSM ) //Character Transmitted
				devCommand = (memaddr*)(devAddrBase + 0xC); // transmit command
			else{
				devStatus = (memaddr*)(devAddrBase + 0x0); //receive status
				devCommand = (memaddr*)(devAddrBase + 0x4); // receive command
			}
			break;
		default : //error 
			//PANIC();
			break;
	}

	/*ack the interrupt*/
	*devCommand= DEV_C_ACK;
	/*if that interrupt represent an incoming internet packet when internet interrupts are abled
		we don't enter this if*/
	if ( line_no != INT_UNUSED || *devStatus < READPENDING){
		w4io = thread_generic_qhead(&io_matrix[line_no-3][device_no]);
		if(w4io != NULL){
			if(w4io->t_status == T_STATUS_W4IO){

				thread_generic_dequeue(&io_matrix[line_no-3][device_no]);
				w4io->t_status = T_STATUS_W4MSG;

				if (msgq_add(SSI, w4io, (uintptr_t)*devStatus) < 0)
					panic_msg("msq_add failed while managing I/O\n"); 
				w4io->t_command = NULL;
				wake_up(SSI,w4io);
			}

			w4io = thread_generic_qhead(&io_matrix[line_no-3][device_no]);
			if ( w4io != NULL ){
				w4io->t_status = T_STATUS_W4IO;
				 __set_dev_fields(w4io->t_command);
			}
		}
	}

	if (tcb == NULL)	schedule();
	else{
		/*running thread is going to finish its time slice*/
		fulfil_timeslice(tcb,timer);
	}
}
