#include "utility.h"

#define DEVTOP 			0x2C0
#define DEVBASE 		0x040
#define DEVRGS			0x10 		//device registers width
#define DISKBASEADDR	DEVBASE    //disk device registers start at DEVBASE 
#define DISKTOPADDR		0x0C0
#define TAPEBASEADDR	DISKTOPADDR
#define TAPETOPADDR		0x140
#define NETBASEADDR		TAPETOPADDR
#define NETTOPADDR		0x1C0
#define PRINTBASEADDR	NETTOPADDR //net top address  is printer base address
#define PRINTTOPADDR	0x240
#define TERMBASEADDR    PRINTTOPADDR
#define TERMTOPADDR		DEVTOP


/*assuming devStatus is correct*/
static inline void get_dev(memaddr devStatus, int* line_no , int *device_no){
	memaddr devAddrBase;

	if ( devStatus < DISKTOPADDR ) *line_no = INT_DISK;
	else if (devStatus < TAPETOPADDR ) *line_no = INT_TAPE;
	else if (devStatus < NETTOPADDR ) *line_no = INT_UNUSED;
	else if (devStatus < PRINTTOPADDR ) *line_no = INT_PRINTER;
	else *line_no = INT_TERMINAL;

	devAddrBase = 0x00000040 + ((*line_no - 3) * 0x80); // *BASEADDR constant
	*device_no = -1;

	while ( devStatus >= devAddrBase ){
		*device_no = *device_no + 1;
		devAddrBase = devAddrBase + DEVRGS;
	}
}

static inline void __get_errno(struct tcb_t* sender){
	int errno = sender->errno;
	msgsend(sender,errno);
}

static inline void __create_process(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc; //parent process
	struct tcb_t* new; // new thread	
	struct { 
		state_t* s;
	} req = {(state_t*)*(++ rinfo)};

	pproc = sender->t_pcb;
	new = thread_alloc(proc_alloc(pproc));
	if (new != NULL) {
		new->t_s = *(req.s);
		new->t_status = T_STATUS_READY;
		thread_enqueue(new, &readyQ);	
		thread_count++;
	}
	msgsend(sender, new);
}

static inline void __create_thread(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc;//parent process
	struct tcb_t* new; // new thread
	struct {
		state_t* s;
	} req = {(state_t*)*(++ rinfo)};
	
	pproc = sender->t_pcb; 
	new = thread_alloc(pproc);
	if (new != NULL) {
		new->t_s = *(req.s);
		new->t_status = T_STATUS_READY;
		thread_enqueue(new, &readyQ);	
		thread_count++;
	}
	msgsend(sender, new);
}

inline void __terminate_process(struct tcb_t* sender){
	struct pcb_t* subroot;
	struct pcb_t* father ;
	struct pcb_t* son ;
	struct tcb_t* tmp;
	int done ;

	subroot = sender->t_pcb;
	son = subroot;
	father = son->p_parent;
	done = 0;

	while( !done ){	
		if (list_empty(&son->p_children)){
			/*delete threads*/
			while(!list_empty(&son->p_threads)){
				tmp = proc_firstthread(son);
				__terminate_thread(tmp);	
				}
			/*delete son*/
			if(son == subroot) done = 1;
			proc_delete(son);
			/*climbing the process tree*/
			son = proc_firstchild(father);
			if (son == NULL){
				son = father;
				father = son->p_parent;
			} 
		} else {
			father = son;
			son = proc_firstchild(father);
		} 
	}
}

inline  void __terminate_thread(struct tcb_t* sender){ 
	struct list_head* iter;
	struct list_head* prev;
	struct tcb_t* tcb;
	uintptr_t temp;

	thread_outqueue(sender);  
	thread_generic_outqueue(sender);
	/*make sender msg queue empty*/
	while(!list_empty(&sender->t_msgq)){
		msgq_get(NULL,sender, &temp);
	}
	thread_count --;
	if (sender->t_status == T_STATUS_W4MSG 
			&& sender->t_wait4sender == SSI) soft_block_count --;
	
	thread_free(sender);
	
	prev = &waitingQ;
	list_for_each(iter,&waitingQ){
		tcb = container_of(iter, struct tcb_t, t_sched);
		if(tcb->t_wait4sender == sender){
			thread_outqueue(tcb);
			tcb->t_status = T_STATUS_READY;
			tcb->t_s.pc = tcb->t_s.pc + 0x4; 
			tcb->t_s.a1 = (uintptr_t)NULL;
			tcb->errno = RECVERR;
			thread_enqueue(tcb, &readyQ);
			}

		if(tcb->t_status == T_STATUS_READY) iter = prev; 
		else prev = iter;		
		}
}

static inline void __setpgmmgr(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc ;//parent process
	struct {
		struct tcb_t* s;
	} req = { (struct tcb_t*)*(++ rinfo)};
	
	pproc = sender->t_pcb;
	
	if(pproc->pgmmgr == NULL ){
		if(req.s != NULL){
			pproc->pgmmgr = req.s;
			msgsend(sender,req.s);
		} else msgsend(sender,NULL);
	} else __terminate_process(sender);
}

static inline void __settlbmgr(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc; //parent process
	struct {
		struct tcb_t * s;
	} req = { (struct tcb_t*)*(++ rinfo)};

	pproc = sender->t_pcb;

	if(pproc->tlbmgr == NULL ){
		if(req.s != NULL){
			pproc->tlbmgr = req.s;
			msgsend(sender,req.s);
		} else msgsend(sender,NULL);
	} else __terminate_process(sender);
}

static inline void __setsysmgr(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc; //parent process
	struct {
		struct tcb_t * s;
	} req = { (struct tcb_t*)*(++ rinfo)};

	pproc = sender->t_pcb;

	if(pproc->sysmgr == NULL){
		if(req.s != NULL){
			pproc->sysmgr = req.s;
			msgsend(sender,req.s);
		} else msgsend(sender,NULL);
	}else __terminate_process(sender);
}

static inline void __get_cputime(struct tcb_t* sender){
	unsigned int time = sender->cputime;
	msgsend(sender, time);
}

static inline void __wait_for_clock(struct tcb_t* sender){
	thread_generic_enqueue(sender,&SSI->t_generic);
}


inline void __set_dev_fields(uintptr_t* rinfo){
	devaddr* devCommand; //device command pointer
	devaddr* devStatus; //device status pointer
	devaddr* devData; //device data pointer
	struct {
			uintptr_t device; //command field address
			uintptr_t command;
			uintptr_t data1;
			uintptr_t data2;
			} req  = { *(rinfo),*(++ rinfo),*(++ rinfo),*(++ rinfo)};
	devCommand = (devaddr*) req.device;
	devStatus = (devaddr*)(req.device-0x4);

	if(req.device < TERMBASEADDR){
		/*if it's not a terminal device*/
		devData = (devaddr*) req.device + 0x4; // DATA0
		*devData = req.data1;
		/*this if is necessary because other devices data1 register is readonly*/
		if (req.device >= NETBASEADDR &&
				req.device < NETTOPADDR){
			/*if it's a network request*/
			devData = (devaddr*) req.device + 0x8; // DATA1
			*devData = req.data2;
		}
		/*starting the I/O operation*/
		*devCommand = req.command;
	} else *devCommand = req.command;
}


static inline void __do_io(uintptr_t* rinfo, struct tcb_t* sender){
	devaddr* devStatus; 
	int line_no;
	int device_no;

	rinfo ++;
	devStatus = (devaddr*)(*rinfo -0x4);
	
	if (*devStatus != DEV_NOT_INSTALLED){
		get_dev((memaddr)devStatus,&line_no,&device_no);

		sender->t_command = rinfo;
		if ( list_empty(&io_matrix[line_no-3][device_no])){
			sender->t_status = T_STATUS_W4IO ;
			__set_dev_fields(rinfo);
		}
		thread_generic_enqueue(sender,&io_matrix[line_no-3][device_no]);
	} else msgsend(sender,DEV_NOT_INSTALLED);
}

static inline void __get_processid(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc ;
	struct {
		struct tcb_t* s;
	} req = {(struct tcb_t*)*(++ rinfo)};
	
	pproc = (req.s)->t_pcb;
	msgsend(sender,pproc);
}

static inline void __get_mythreadid(struct tcb_t* sender){
	msgsend(sender,sender);
}

static inline void __get_parentprocid(uintptr_t* rinfo,struct tcb_t* sender){
	struct pcb_t* pproc ;
	struct {
		struct pcb_t* s;
	} req = {(struct pcb_t*)*(++ rinfo)};

	pproc = (req.s)->p_parent ;
	msgsend(sender,pproc);
}

void SSI_function_entry_point(void){
	uintptr_t service;
	struct tcb_t* sender;
	
	while(1){	
		sender = msgrecv(NULL,&service); 
		/*if the sender is not dead*/
		if(!list_empty(&sender->t_sched)){ // && dest->t_status = T_STATUS_NONE
			/*request from a tcb*/
			switch(*(uintptr_t*)service){
				case GET_ERRNO:
					__get_errno(sender);
					break;
				case CREATE_PROCESS:
					__create_process((uintptr_t*)service,sender);
					break;
				case CREATE_THREAD: 
					__create_thread((uintptr_t*)service,sender);		
					break;  
				case TERMINATE_PROCESS:
					__terminate_process(sender);
					break;
				case TERMINATE_THREAD:
					__terminate_thread(sender);
					break;
				case SETPGMMGR:
					__setpgmmgr((uintptr_t*)service,sender);
					break;
				case SETTLBMGR:
					__settlbmgr((uintptr_t*)service,sender);
					break;
				case SETSYSMGR:
					__setsysmgr((uintptr_t*)service,sender);
					break;
				case GET_CPUTIME:
					__get_cputime(sender);
					break;
				case WAIT_FOR_CLOCK:
					__wait_for_clock(sender);
					break;
				case DO_IO:				
					__do_io((uintptr_t*)service,sender);
					break; 
				case GET_PROCESSID: 
					__get_processid((uintptr_t*)service,sender);
					break;					
				case GET_MYTHREADID:
					__get_mythreadid(sender);
					break;
				case GET_PARENTPROCID:
					__get_parentprocid((uintptr_t*)service,sender);
					break;
				default : //error 
					__terminate_process(sender);
					break;
			}
		}
	}
}