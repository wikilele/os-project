#ifndef MIKABOOQ_H
#define MIKABOOQ_H
#include <listx.h>
#include <uARMtypes.h>

typedef unsigned int uintptr_t ;

struct pcb_t {
	struct pcb_t * p_parent ; /* pointer to parent */
	struct tcb_t * pgmmgr;
	struct tcb_t * tlbmgr;
	struct tcb_t * sysmgr;
	struct list_head p_threads; /* list of threads */

	struct list_head p_children; /* list of children (hierarchy of processes) */
	struct list_head p_siblings; /* link the other siblings (children of p_parent) */
};

#define T_STATUS_NONE  0    /* unused thread descriptor */
#define T_STATUS_READY 1
#define T_STATUS_W4MSG 4
#define T_STATUS_W4IO  5    

struct tcb_t {
	struct pcb_t *t_pcb; /* pointer to the process */
	state_t t_s ; /* processor state */

	int t_status;
	unsigned int cputime;
	int errno;
	uintptr_t* t_command;
	
	struct tcb_t *t_wait4sender; /* expected sender (if t_status == T_STATUS_W4MSG), NULL means accept msg from anybody */
	struct list_head t_next; /* link the other elements of the list of threads in the same process */
	struct list_head t_sched; /* link the other elements on the same scheduling list */
	struct list_head t_msgq; /* list of pending messages for the current thread */
	struct list_head t_generic; /*used for psudoclock purpose and i/o management*/
};

struct msg_t {
	struct tcb_t *m_sender; /* sender thread */
	uintptr_t m_value; /* payload of the message */

	struct list_head m_next; /* link the other elements of the pending message queue */
};

/************************************** PROC MGMT ************************/

/* initialize the data structure */
/* the return value is the address of the root process */
struct pcb_t *proc_init(void);

/* alloc a new empty pcb (as a child of p_parent) */
/* p_parent cannot be NULL */
struct pcb_t *proc_alloc(struct pcb_t *p_parent);

/* delete a process (properly updating the process tree links) */
/* this function must fail if the process has threads or children. */
/* return value: 0 in case of success, -1 otherwise */
int proc_delete(struct pcb_t *oldproc);

/* return the pointer to the first child (NULL if the process has no children) */
struct pcb_t *proc_firstchild(struct pcb_t *proc);

/* return the pointer to the first thread (NULL if the process has no threads) */
struct tcb_t *proc_firstthread(struct pcb_t *proc);

/****************************************** THREAD ALLOCATION ****************/

/* initialize the data structure */
void thread_init(void);

/* alloc a new tcb (as a thread of process) */
/* return -1 if process == NULL or mo more available tcb-s.
	 return 0 (success) otherwise */
struct tcb_t *thread_alloc(struct pcb_t *process);

/* Deallocate a tcb (unregistering it from the list of threads of
	 its process) */
/* it fails if the message queue is not empty (returning -1) */
int thread_free(struct tcb_t *oldthread);

/*************************** THREAD QUEUE ************************/

/* add a tcb to the scheduling queue */
void thread_enqueue(struct tcb_t *new, struct list_head *queue);

/* return the head element of a scheduling queue.
	 (this function does not dequeues the element)
	 return NULL if the list is empty */
struct tcb_t *thread_qhead(struct list_head *queue);

/* get the first element of a scheduling queue.
	 return NULL if the list is empty */
struct tcb_t *thread_dequeue(struct list_head *queue);

static inline void thread_outqueue(struct tcb_t *this) {
	list_del(&this->t_sched);
}

#define for_each_thread_in_q(pos, queue) \
	list_for_each_entry(pos, queue, t_sched)

/***************** PSEUDOCLOCK QUEUE and I/O MANAGEMENT **********************/
/* add a tcb to a t_generic queue */
void thread_generic_enqueue(struct tcb_t *new, struct list_head *queue);

/*head of a t_generic queue  */
struct tcb_t *thread_generic_qhead(struct list_head *queue);

/* get the first element of the t_generic queue.
	 return NULL if the list is empty */
struct tcb_t *thread_generic_dequeue(struct list_head *queue);

static inline void thread_generic_outqueue(struct tcb_t *this) {
	list_del(&this->t_generic);
}

/********************** SSI PUSH FUNCTION *********************/

/* push the SSI to the scheduling ready queue */
/*this function must be called with (SSI, &readyQ)*/
void ssi_push(struct tcb_t *new, struct list_head *queue);

/*************************** MSG QUEUE ************************/

/* initialize the data structure */
/* the return value is the address of the root process */
void msgq_init(void);

/* add a message to a message queue. */
/* msgq_add fails (returning -1) if no more msgq elements are available */
int msgq_add(struct tcb_t *sender, struct tcb_t *destination, uintptr_t value);

/* retrieve a message from a message queue */
/* -> if sender == NULL: get a message from any sender
	 -> if sender != NULL && *sender == NULL: get a message from any sender and store
	 the address of the sending tcb in *sender
	 -> if sender != NULL && *sender != NULL: get a message sent by *sender */
/* return -1 if there are no messages in the queue matching the request.
	 return 0 and store the message payload in *value otherwise. */
int msgq_get(struct tcb_t **sender, struct tcb_t *destination, uintptr_t *value);


#endif
