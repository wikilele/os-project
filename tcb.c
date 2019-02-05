/*Implementation of functions for Thread Control Block structure */
#include "const.h"
#include "listx.h"
#include "mikabooq.h"

/*static array for tcb_t allocation*/
static struct tcb_t threadsArray[MAXTHREAD];

/*sentinel of the free list*/
/*LIFO structure*/
LIST_HEAD(tstackHead);

/****************************************** THREAD ALLOCATION ****************/

/* initialize the data structure */
/*we assume that inside the free list structures have all fields properly initialized */
void thread_init(void){
	int i;
	/*intializing the free list */
		/*from MAXTHREAD-1 to 0 because pstackHead is a LIFO structure*/
		for(i=MAXTHREAD-1;i>= 0;i--)
		{
			/*intitialize threadsArray[i]*/
			threadsArray[i].t_pcb = NULL;
			//threadsArray[i].t_s  
			threadsArray[i].t_status = T_STATUS_NONE;
			threadsArray[i].cputime = 0;
			threadsArray[i].errno = 0;
			threadsArray[i].t_command = NULL;
			threadsArray[i].t_wait4sender = NULL;
			//INIT_LIST_HEAD(&threadsArray[i].t_next);
			INIT_LIST_HEAD(&threadsArray[i].t_sched);
			INIT_LIST_HEAD(&threadsArray[i].t_msgq);
			INIT_LIST_HEAD(&threadsArray[i].t_generic);

			list_add(&threadsArray[i].t_next,&tstackHead);
		}
	return;
}

/* alloc a new tcb (as a thread of process) */
/* return -1 if process == NULL or no more available tcb-s.
	 return 0 (success) otherwise */
struct tcb_t *thread_alloc(struct pcb_t *process){
	struct tcb_t *new;  /* pointer to the element taken from the free list*/
	/*checking process and free list*/
		if(process == NULL ||
			list_empty(&tstackHead)) return NULL ;
	/* else take and pop one element from the free list*/
		else
		{
			new = container_of(tstackHead.next,struct tcb_t,t_next);
			list_del(tstackHead.next);
		/*insert as a thread of process*/
			new->t_pcb = process;
			list_add_tail(&new->t_next,&process->p_threads);
		/*return the pointer to the thread*/
			return new ;
		}
}

/* Deallocate a tcb (unregistering it from the list of threads of
	 its process) */
/* it fails if the message queue is not empty (returning -1) */
/*this function won't be called when oldtrhead is inside the scheduler queue*/
int thread_free(struct tcb_t *oldthread){
	/*checking if there is at least  one message*/
		if(!list_empty(&oldthread->t_msgq)) return -1 ;
	/*else remove it from the threads list of the process*/
		else
		{
			list_del(&oldthread->t_next);
		/*readd the thread in the free list*/
		/*reinitialize the fieds in the right way*/
			oldthread->t_pcb = NULL;
			//oldthread->t_s
			oldthread->t_status = T_STATUS_NONE;
			oldthread->cputime = 0;
			//oldthread->errno = 0;
			//oldthread->t_command = NULL;
			oldthread->t_wait4sender = NULL;
			//INIT_LIST_HEAD(&oldthread->t_next);
			INIT_LIST_HEAD(&oldthread->t_sched);
			//INIT_LIST_HEAD(&oldthread->t_msgq);
			INIT_LIST_HEAD(&oldthread->t_generic);
			
			list_add(&oldthread->t_next,&tstackHead);
		/*if everithing it's ok return 0*/
			return 0;
		}
}

/*************************** THREAD QUEUE ************************/

/* add a tcb to the scheduling queue */
void thread_enqueue(struct tcb_t *new, struct list_head *queue){
	/*insert new in the queue*/
		list_add_tail(&new->t_sched,queue);
	return ;
}

/* return the head element of a scheduling queue.
	 (this function does not dequeues the element)
	 return NULL if the list is empty */
struct tcb_t *thread_qhead(struct list_head *queue){
	/*cheching if the queue is empty*/
		if (list_empty(queue)) return NULL ;
	/*take the top element and return the pointer to the entire structure*/
		else 
		{
		/*the element is not dequeued*/	
			return container_of(queue->next,struct tcb_t,t_sched);
		}
}

/* get the first element of a scheduling queue.
	 return NULL if the list is empty */
struct tcb_t *thread_dequeue(struct list_head *queue){
	struct tcb_t *old; /*pointer to the element popped from the scheduling queue*/
	/*cheching if the queue is empty*/
		if(list_empty(queue)) return NULL;
	/*take and remove the top element, return the pointer to the entire structure*/
		else 
		{
			old = container_of(queue->next,struct tcb_t,t_sched);
			list_del(queue->next);
		/*return this element*/
			return old ;
		}
}

/******************** PSEUDOCLOCK QUEUE and I/O MANAGEMENT **********************************/

/* add a tcb to the t_generic queue */
void thread_generic_enqueue(struct tcb_t *new, struct list_head *queue){
	/*insert new in the queue*/
		list_add_tail(&new->t_generic,queue);
	return ;
}

/*head of a t_generic queue  */
struct tcb_t *thread_generic_qhead(struct list_head *queue){
	/*cheching if the queue is empty*/
		if (list_empty(queue)) return NULL ;
	/*take the top element and return the pointer to the entire structure*/
		else 
		{
		/*the element is not dequeued*/	
			return container_of(queue->next,struct tcb_t,t_generic);
		}
}


/* get the first element of the t_generic queue.
	 return NULL if the list is empty */
struct tcb_t *thread_generic_dequeue(struct list_head *queue){
	struct tcb_t *old; /*pointer to the element popped from the scheduling queue*/
	/*cheching if the queue is empty*/
		if(list_empty(queue)) return NULL;
	/*take and remove the top element, return the pointer to the entire structure*/
		else 
		{
			old = container_of(queue->next,struct tcb_t,t_generic);
			list_del(queue->next);
		/*return this element*/
			return old ;
		}
}


void ssi_push(struct tcb_t *new, struct list_head *queue){
	/*insert new in the queue*/
	list_add(&new->t_sched,queue);
	return ;
}