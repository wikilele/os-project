/*Implementation of functions for Message structure */
#include "const.h"
#include "listx.h"
#include "mikabooq.h"

static void addokbuf(char *strp) {
	  tprint(strp);
}

/*static array for msg_t allocation*/
static struct msg_t messagesArray[MAXMSG];

/*sentinel of the free list*/
/*LIFO structure*/
LIST_HEAD(mstackHead);

/* initialize the data structure */
/* the return value is the address of the root process */
/*we assume that inside the free list structures have all fields properly initialized */
void msgq_init(void){
	int i ;
	/*initializing the free list*/
		/*from MAXMSG-1 to 0 because pstackHead is a LIFO structure*/
		for(i = MAXMSG-1;i >= 0; i--)
		{
			/*initialize messagesArray[i]*/
			messagesArray[i].m_sender = NULL;
			//messagesArray[i].m_value
			//INIT_LIST_HEAD(&messagesArray[i].m_next);  

			list_add(&messagesArray[i].m_next,&mstackHead);
		}
	return ;
}

/* add a message to a message queue. */
/* msgq_add fails (returning -1) if no more msgq elements are available */
int msgq_add(struct tcb_t *sender, struct tcb_t *destination, uintptr_t value){
	struct msg_t *new ;		/* pointer to the element taken from the free list*/
	/*checking sender,destination and the free list*/
		if (sender == NULL ||
				destination == NULL ||
					list_empty(&mstackHead)) return -1;
	/*else take and remove one element from the free list*/
		else
		{
			new = container_of(mstackHead.next , struct msg_t , m_next);
			list_del(mstackHead.next);
		/*initialize its fields*/
			new->m_sender = sender ;
			new->m_value = value ;
		/*add it to msg queue of destination*/
			list_add_tail(&new->m_next,&destination->t_msgq);
		/*return 0 if everything's gone well*/
			return 0;
		}	  
}

/* retrieve a message from a message queue */
/* -> if sender == NULL: get a message from any sender
	 -> if sender != NULL && *sender == NULL: get a message from any sender and store
	 the address of the sending tcb in *sender
	 -> if sender != NULL && *sender != NULL: get a message sent by *sender */
/* return -1 if there are no messages in the queue matching the request.
	 return 0 and store the message payload in *value otherwise. */
int msgq_get(struct tcb_t **sender, struct tcb_t *destination, uintptr_t *value){

	int found = 0; /*boolean flag*/
	struct msg_t *new ; /*pointer to the right msg*/
	struct list_head *iter; /*pointer used to serach the right msg in the queue*/
	/*if sender == NULL*/
	if (sender == NULL)
	{
		/*take and remove  the first msg of the queue*/
		if (list_empty(&destination->t_msgq)) return -1 ;
		else
		{
			new = container_of((destination->t_msgq).next,struct msg_t , m_next);
			/*get only the payload*/
			*value = new->m_value;
		}
	}
	/*else if sender != NULL && *sender == NULL */
	else if (sender != NULL && *sender == NULL)
	{
		/*take and remove  the first msg of the queue*/
		if (list_empty(&destination->t_msgq))return -1;
		else
		{
			new = container_of((destination->t_msgq).next,struct msg_t,m_next);
		/*get the payload and the sender*/
			*value = new->m_value;
			*sender = new->m_sender; 
		}
	}
	/*else if sender != NULL && *sender != NULL*/
	else if (sender != NULL && *sender != NULL)
	{
		iter = (destination->t_msgq).next;
		while (iter != &destination->t_msgq && !found)
		{
			new = container_of(iter,struct msg_t,m_next);
			if (new->m_sender == *sender) found = 1 ;
			else iter = iter->next ;
		}
		if (!found ) return -1;
		// else *value = element->m_value
		else *value = new->m_value ;
	}
	/*else return -1*/
	else return -1;
	/*recover the msg_t element*/ 
		list_del(&new->m_next);
		/*reinitialize its fields*/
		new->m_sender = NULL;
		//new->m_value
		//INIT_LIST_HEAD(&new->m_next);

		list_add(&new->m_next,&mstackHead);
	/*can finally return 0*/
		return 0; 
}


