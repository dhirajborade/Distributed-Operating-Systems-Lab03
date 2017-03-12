/*  main.c  - main */

/* Lab03 Original */

#include <xinu.h>

#define MAXTOPICS 256
#define MAXSUB 8

/* Topic Table Structure */
struct topicStruct
{
	struct subscriberStruct* subscriberHead;
	int32 subscriberCount;
};

/* Topic Subscriber List Structure */
struct subscriberStruct
{
	pid32 pid;
	topic16 group;
	void (*handlerPointer)(topic16, uint32);
	struct subscriberStruct* next;
};

struct topicStruct topicTable[MAXTOPICS];
sid32 topicTableSem;

/* Broker List Structure */
struct brokerList
{
	uint32 data;
	topic16 topic;
	struct brokerList* next;
};

struct brokerList* brokerHead;
sid32 consumer;
sid32 producer;

status init_topicTable()
{
	int32 i;
	for(i = 0 ; i < MAXTOPICS ; i++)
	{
		topicTable[i].subscriberCount = 0;
		topicTable[i].subscriberHead = (struct subscriberStruct *) NULL;
	}
	if((topicTableSem = semcreate(1)) == SYSERR)
		return SYSERR;
	return OK;
}

status init_broker()
{
	brokerHead = (struct brokerList *) getmem (sizeof(struct brokerList));
	brokerHead -> next = (struct brokerList*) NULL;
	brokerHead -> data = 0;
	brokerHead -> topic = 0;
	if((producer = semcreate(MAXSUB)) == SYSERR) return SYSERR;
	if((consumer = semcreate(0)) == SYSERR) return SYSERR;
	return OK;
}

/* Subscribe the process to the Topic */
syscall subscribe(topic16 topic, void (*handler)(topic16, void* , uint32))
{
	intmask mask;
	struct topicStruct* topicStructEntry;
	struct subscriberStruct* subscriberStructEntry;
	mask = disable();

	/* Check whether the Topic ID is within the range */
	if(topic & 0x00FF < 0 || topic & 0x00FF >= MAXTOPICS)
	{
		restore(mask);
		return SYSERR;
	}

	/* Check whether the number of subsciber for one topic is within limit */
	topicStructEntry = &topicTable[topic & 0x00FF];
	wait(topicTableSem);
	if(topicStructEntry -> subscriberCount >= MAXSUB)
	{
		signal(topicTableSem);
		restore(mask);
		return SYSERR;
	}
	/* Subscribe the process to the Topic */
	subscriberStructEntry = topicStructEntry -> subscriberHead;
	struct subscriberStruct *newSubscriber = (struct subscriberStruct *) getmem (sizeof(struct subscriberStruct));
	
	if ((int32)newSubscriber == SYSERR)
	{
		signal(topicTableSem);
		restore(mask);
		return SYSERR;
	}

	newSubscriber -> pid = currpid;
	newSubscriber -> handlerPointer = handler;
	newSubscriber -> next = subscriberStructEntry;
	newSubscriber -> group = topic >> 8;
	topicStructEntry -> subscriberHead = newSubscriber;
	signal(topicTableSem);
	restore(mask);
	return OK;
}

/* Unsubscribe the process from the Topic */
syscall unsubscribe(topic16 topic)
{
	intmask mask;
	struct topicStruct* topicStructEntry;
	struct subscriberStruct* subscriberStructEntry;
	mask=disable();

	/* Check whether the Topic ID is within the range */
	if(topic & 0x00FF < 0 || topic & 0x00FF >= MAXTOPICS)
	{
		restore(mask);
		return SYSERR;
	}
	topicStructEntry = &topicTable[topic & 0x00FF];
	wait(topicTableSem);
	subscriberStructEntry = topicStructEntry -> subscriberHead;
	
	/* Traverse down the list to get the subscriber of currpid */
	while(subscriberStructEntry != (struct subscriberStruct*) NULL && subscriberStructEntry -> pid != currpid)
		subscriberStructEntry = subscriberStructEntry -> next;

	/* currpid not found in the subscriber struct */
	if(subscriberStructEntry == (struct subscriberStruct*) NULL)
	{
		signal(topicTableSem);
		restore(mask);
		return SYSERR;
	}

	/* Remove subscriberStruct */
	if(subscriberStructEntry -> next == (struct subscriberStruct*) NULL)
	{
		if(freemem(subscriberStructEntry, sizeof(struct subscriberStruct)) == SYSERR)
		{
			signal(topicTableSem);
			restore(mask);
			return SYSERR;
		}
	}
	else
	{
		subscriberStructEntry -> pid = subscriberStructEntry -> next -> pid;
		subscriberStructEntry -> handlerPointer = subscriberStructEntry -> next -> handlerPointer;
		struct subscriberStruct* tmp;
		tmp = subscriberStructEntry -> next;
		subscriberStructEntry -> next = subscriberStructEntry -> next -> next;
		if(freemem((char *)tmp, sizeof(struct subscriberStruct)) == SYSERR)
		{
			signal(topicTableSem);
			restore(mask);
			return SYSERR;
		}
	}
	signal(topicTableSem);
	restore(mask);
	return OK;
}

/* Publish the Topic data to the Broker */
syscall publish(topic16 topic, uint32 data)
{	
	intmask mask;
	struct brokerList* brokerEntry;
	mask=disable();

	/* Check whether the Topic ID is within the range */
	if(topic & 0x00FF < 0 || topic & 0x00FF >= MAXTOPICS)
	{
		restore(mask);
		return SYSERR;
	}
	wait(producer);
	struct brokerList *newBroker = (struct brokerList *) getmem (sizeof(struct brokerList));
	if ((int32)newBroker == SYSERR)
	{
		signal(producer);
		restore(mask);
		return SYSERR;
	}
	/* void *newData = getmem(sizeof(data) * size);
	if ((int32)newData == SYSERR)
	{
		signal(producer);
		restore(mask);
		return SYSERR;
	}*/
	//memcpy(newData, data, sizeof(data) * size);
	newBroker -> data = data;
	//newBroker -> size = size;
	newBroker -> topic = topic;
	newBroker -> next = (struct brokerList*) NULL;
	brokerEntry = brokerHead;
	while(brokerEntry -> next != (struct brokerList*) NULL)
		brokerEntry = brokerEntry -> next;
	brokerEntry -> next = newBroker;
	signal(consumer);
	restore(mask);
	return OK;
}

void handler1(topic16 topic, uint32 data)
{
	printf("- Function handler1() is called with topic16 0x%04x and data %d\n", topic & 0xFFFF, data);
}

void handler2(topic16 topic, uint32 data)
{
	printf("- Function handler2() is called with topic16 0x%04x and data %d\n", topic & 0xFFFF, data);
}

syscall unsubscribeAll()
{
	int32 i;
	for(i = 0 ; i < MAXTOPICS ; i++)
		unsubscribe(i);
	return OK;
}

process A()
{
	printf("Start Process A\n");
	topic16 topic;
	topic = 0x013F;
	if(subscribe(topic, &handler1) == SYSERR)
		printf("Failed to subscribe\n");
	else
		printf("Process A subscribes with a topic16 value of 0x%04x and handler1\n", topic);
	sleep(20);
	unsubscribeAll();
	return OK;
}

process B()
{
	printf("Start Process B\n");
	topic16 topic;
	topic = 0x023F;
	if(subscribe(topic, &handler2) == SYSERR)
		printf("Failed to subscribe\n");
	else
		printf("Process A subscribes with a topic16 value of 0x%04x and handler2\n",topic);
	sleep(20);
	unsubscribeAll();
	return OK;
}

process C()
{
	printf("Start Process C\n");
	sleep(1);
	topic16 topic;
	uint32 data = 100;
	//uint32 size = sizeof(data)/sizeof(data[0]);
	topic = 0x013F;
	printf("Process C publishes data %s to topic16 0x%04x\n", data, topic);
	publish(topic, data);
	return OK;
}

process D()
{
	printf("Start Process D\n");
	sleep(1);
	topic16 topic;
	uint32 data = 200;
	//uint32 size = sizeof(data)/sizeof(data[0]);
	topic = 0x003F;
	printf("Process D publishes data %s to topic16 0x%04x\n", data, topic);
	publish(topic, data);
	return OK;
}

process Broker()
{
	struct brokerList* brokerEntry;
	struct subscriberStruct* subscriberEntry;
	printf("Start Broker\n");
	while(1)
	{
		wait(consumer);
		brokerEntry = brokerHead->next;
		wait(topicTableSem);
		subscriberEntry = topicTable[brokerEntry -> topic & 0x00FF].subscriberHead;
		while(subscriberEntry != (struct subscriberStruct *) NULL)
		{
			if(brokerEntry -> topic >> 8 != 0 && subscriberEntry -> group != brokerEntry -> topic >> 8)
			{
				subscriberEntry = subscriberEntry -> next;
				continue;
			}
			subscriberEntry -> handlerPointer(brokerEntry -> topic, brokerEntry -> data);
			subscriberEntry = subscriberEntry -> next;
		}
		signal(topicTableSem);
		brokerHead -> next = brokerEntry -> next;
		freemem(brokerEntry, sizeof(struct brokerList));
		signal(producer);
		yield();
	}
	return OK;
}

process	main(void)
{
	recvclr();
	
	/* Initialize the Topic Table */
	if(init_topicTable() == SYSERR)
	{
		printf("Failed to initialize topic table");
		return SYSERR;
	}

	/* Initialize the Broker List */
	if(init_broker() == SYSERR)
	{
		printf("Failed to initialize broker list");
		return SYSERR;
	}
	
	resume(create(A, 4096, 50, "A", 0)); // Process A created as a subscriber
	resume(create(B, 4096, 50, "B", 0)); // Process B created as a subscriber
	resume(create(C, 4096, 50, "C", 0)); // Process C created as a publisher
	resume(create(D, 4096, 50, "D", 0)); // Process D created as a publisher
	resume(create(Broker,4096, 50, "Broker", 0));
	printf("All processes have finished executing\n");
	return OK;
}
