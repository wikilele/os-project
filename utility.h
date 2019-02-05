#ifndef UTILITY_H
#define UTILITY_H

#include <uARMconst.h>
#include <uARMtypes.h>
#include <libuarm.h>

#include "const.h"
#include "listx.h"
#include "mikabooq.h"
#include "nucleus.h"

#define TIMESLICE 5000 /*microseconds*/

/*error numbers*/
#define SUCCESS		 	0
#define SENDERR			-1
#define RECVERR			-2
#define SYSCALLERR		-3


struct tcb_t* SSI;
struct pcb_t* root;

struct list_head readyQ;
struct list_head running; //fatto cosi per coerenza con le altre code
struct list_head waitingQ;


unsigned int thread_count;
unsigned int soft_block_count; //number of blocked threads awaiting for I/O or completion of aservice request by the SSI
struct list_head io_matrix[DEV_USED_INTS][DEV_PER_INT]; // line_no -3 * device_no

void SSI_function_entry_point();


void SysBpExceptionHandler(void); 
void ProgTrapExceptionHandler(void); 
void TLBTrapExceptionHandler(void);  
void InterruptExceptionHandler(void);


extern void schedule();
extern void test(void); 


inline void __terminate_thread(struct tcb_t* sender);
inline void __terminate_process(struct tcb_t* sender);
inline void __set_dev_fields(uintptr_t* rinfo);

static void panic_msg(char *s){
	setSTATUS(STATUS_ALL_INT_DISABLE(getSTATUS()));
	tprint(s);
	PANIC();
}

#endif