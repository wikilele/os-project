/*Implementation of functions for Process Control Block structure */
#include "const.h"
#include "listx.h"
#include "mikabooq.h"

/*static array for pcb_t allocation*/
/*static attribute : its lifetime extends across the entire run of the program */
/*static global variable or a function is "seen" only in the file it's declared in*/
static struct pcb_t processesArray[MAXPROC];

/*sentinel of the free list*/
/*LIFO structure*/
LIST_HEAD(pstackHead);

/* initialize the data structure */
/* the return value is the address of the root process */
/*we assume that inside the free list, structures have all fields properly initialized */
struct pcb_t *proc_init(void){
	int i ;
	/*initializing the free list*/
		/*from MAXPROC-1 to 0 because pstackHead is a LIFO structure*/
		for(i = MAXPROC-1; i >= 0 ; i --)
		{
			/*initialize fields of processesArray[i]*/
			processesArray[i].p_parent = NULL;
			processesArray[i].pgmmgr = NULL;
			processesArray[i].tlbmgr = NULL;
			processesArray[i].sysmgr = NULL;
			INIT_LIST_HEAD(&processesArray[i].p_threads); 
			INIT_LIST_HEAD(&processesArray[i].p_children); 
			//INIT_LIST_HEAD(&processesArray[i].p_siblings); 
			
			list_add(&processesArray[i].p_siblings,&pstackHead);
		}
	/*pop the first element of the free list*/
		list_del(&processesArray[0].p_siblings);
	/*return the pointer to the element taken*/
		return(&processesArray[0]);
}

/* alloc a new empty pcb (as a child of p_parent) */
/* p_parent cannot be NULL */
struct pcb_t *proc_alloc(struct pcb_t *p_parent){
	struct pcb_t *new ;  /* pointer to the element taken from the free list*/
	/*checking p_parent and the free list*/
		if(p_parent == NULL ||
			list_empty(&pstackHead))	return NULL;
	/*else take the fisrt element of the free list and pop it*/
		else 
		{
			new = container_of(pstackHead.next,struct pcb_t,p_siblings);  
			list_del(pstackHead.next);
		/*add it as a son of p_parent*/
			new->p_parent=p_parent;
			list_add_tail(&new->p_siblings,&p_parent->p_children);
		/*return the pointer to the element added*/
			return new ;
		}
}

/* delete a process (properly updating the process tree links) */
/* this function must fail if the process has threads or children. */
/* return value: 0 in case of success, -1 otherwise */
int proc_delete(struct pcb_t *oldproc){
	/*cheking if there are any children or threads*/
		if (!list_empty(&oldproc->p_children) ||
				!list_empty(&oldproc->p_threads))	return -1;  
	/*else remove it from the hierarchy*/
		else 
		{
			list_del(&oldproc->p_siblings);
		/*readd it to the free list reinitializing its fields*/
			oldproc->p_parent = NULL;
			oldproc->pgmmgr = NULL;
			oldproc->tlbmgr = NULL;
			oldproc->sysmgr = NULL;
			//INIT_LIST_HEAD(&oldproc->p_threads); 
			//INIT_LIST_HEAD(&oldproc->p_children); 
			//INIT_LIST_HEAD(&oldproc->p_siblings); 
		
			list_add(&oldproc->p_siblings,&pstackHead);
		/*if everything's gone fine return 0*/
			return 0 ;
		}
}

/* return the pointer to the first child (NULL if the process has no children) */
struct pcb_t *proc_firstchild(struct pcb_t *proc){
	/*checking if there is at least one child*/
		if (list_empty(&proc->p_children)) return NULL;
	/*else returning the pointer to the entire structure*/
		else 
		{
			return container_of((proc->p_children).next,struct pcb_t,p_siblings);
		}
}

/* return the pointer to the first thread (NULL if the process has no threads) */
struct tcb_t *proc_firstthread(struct pcb_t *proc){
	/*checking if there is at least one thread*/
		if (list_empty(&proc->p_threads)) return NULL;
	/*else returning the pointer to the entire structure*/
		else
		{
			return container_of((proc->p_threads).next,struct tcb_t,t_next);
		}
}
