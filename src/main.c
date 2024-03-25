/*

Deadline-Driven Scheduler (EDF)

DD-Task:
	- Task managed by DDS
	- Data structure that holds hanlde of corresponding user-defined F-Task

F-Task:
	- Task managed by FreeRTOS

DD-Task Lists:
	1. Active Task List
	   - A list of DD-Tasks which the DDS currently needs to schedule.
	   - Needs to be sorted by deadline every time a DD-Task is added or removed

	2. Completed Task List
	   - A list of DD-Tasks which have completed execution before their deadlines.
	   - Primarily used for debugging/testing, not used in practice due to overhead

	3. Overdue Task List
	   - A list of DD-Tasks which have missed their deadlines

	* DD-Tasks which successfully complete their execution before their deadline must be removed from the
	  Active Task List and added to the Completed Task List. DD-Tasks which do not meet their deadlines must
	  be removed from the Active Task List and added to the Overdue Task List.

	* Can be returned by Refference or Value, need to justify
	  --> RETURN BY REFERENCE

Main Tasks (4):
	1. Deadline-Driven Scheduler (Priority: 1)
	   - Implements the EDF algorithm and controls the priorities of user-defined F-tasks from an activelymanaged list of DD-Tasks.
	   - Set prioritie of referenced F-Task to 'high', others to 'low'

	Auxillary F-Tasks (Testing):

	2. User-Defined Tasks (Priority: 3)
	   - Contains the actual deadline-sensitive application code written by the user.
	   - Must call complete_dd_task once it hs finished

	3. Deadline-Driven Task Generator (Priority: 3)
	   - Periodically creates DD-Tasks that need to be scheduled by the DD Scheduler.
	   - Normally suspended, resumed whenever a timer callback is triggered
	   - Timer should be configured to expire based on particular DD-Tasks time period
	   - Prepares all nexesary info for creating specific instances of DD-Tasks, then calls release_dd_task
	   * Can use single generator to create all DD-Tasks or Multiple generators
		 --> MULTIPLE GENERATORS
	   * F-Task handles stored inside each DD-Task may be either created once when app is initialized and re-used
		 OR F-Task handles continuously created and deleted every time a DD-Task is released and completed (FreeRTOS need to be configured to use heap_4.c instead of heap_1.c for this)
		 --> CREATED ONCE, heap_1.c

	4. Monitor Task (Priority: 4)
	   - F-Task to extract information from the DDS and report scheduling information.
	   - Responsible for:
		1) Number of DD-Tasks
		2) Number of completed DD-Tasks
		3) Number of overdue DD-Tasks
	   - Collects info from DDS using:
		1) get_active_dd_task_list
		2) get_complete_dd_task_list
		3) get_overdue_dd_task_list
	   - Print Number of tasks to console
	   - Must be allowed to execute even if there are active or overdue tasks

	* All 3 Aux tasks should not have access to any internal DS, only interface with DDS via 4 main functions

Core Functionality:

	1. 	release_dd_task

	This function receives all of the information necessary to create a new dd_task struct (excluding
	the release time and completion time). The struct is packaged as a message and sent to a queue
	for the DDS to receive.

	2. 	complete_dd_task

	This function receivesthe ID of the DD-Task which has completed its execution. The ID is packaged
	as a message and sent to a queue for the DDS to receive.

	3. 	get_active_dd_task_list

	This function sends a message to a queue requesting the Active Task List from the DDS. Once a
	response is received from the DDS, the function returns the list.

	4. 	get_completed_dd_task_list

	This function sends a message to a queue requesting the Completed Task List from the DDS. Once
	a response is received from the DDS, the function returns the list.

	5. 	get_overdue_dd_task_list

	This function sends a message to a queue requesting the Overdue Task List from the DDS. Once a
	response is received from the DDS, the function returns the list

*/

/* Custom includes. */
#include "dd_task_list.h"

#define PRIORITY_HIGH 4
#define PRIORITY_MED 3
#define PRIORITY_LOW 1
#define MESSAGE_QUEUE_SIZE 50

/* TEST BENCH */
#define TEST_BENCH 1

#ifdef TEST_BENCH
#if TEST_BENCH == 1
/* Test Bench #1 */
#define t1_execution 95
#define t1_period 500
#define t2_execution 150
#define t2_period 500
#define t3_execution 250
#define t3_period 750
#elif TEST_BENCH == 2
/* Test Bench #2 */
#define t1_execution 95
#define t1_period 250
#define t2_execution 150
#define t2_period 500
#define t3_execution 250
#define t3_period 750
#elif TEST_BENCH == 3
/* Test Bench #3 */
#define t1_execution 100
#define t1_period 500
#define t2_execution 200
#define t2_period 500
#define t3_execution 200
#define t3_period 500
#else
#error "Invalid test bench specified"
#endif
#endif
typedef enum message_type message_type;
enum message_type
{
	release,
	complete,
	get_active,
	get_completed,
	get_overdue
};

typedef struct dd_message
{
	dd_task task;
	message_type type;
	dd_task_node *list;
} dd_message;

/* Prototypes. */
// Task handles ** might not need **
TaskHandle_t pxDDS;
TaskHandle_t pxMonitor;
TaskHandle_t pxUser;
TaskHandle_t pxTaskGen1;
TaskHandle_t pxTaskGen2;
TaskHandle_t pxTaskGen3;
void myDDS_Init();
void dd_scheduler(void *pvParameters);
void dd_task_generator_1(void *pvParameters);
void dd_task_generator_2(void *pvParameters);
void dd_task_generator_3(void *pvParameters);
void user_defined(void *pvParameters);
void monitor(void *pvParameters);
void release_dd_task(TaskHandle_t t_handle,
					 task_type type,
					 uint32_t task_id,
					 uint16_t task_number);
int get_execution_time(uint16_t task_number);
int get_period(uint16_t task_number);

void complete_dd_task(uint32_t task_id);
dd_task_node **get_active_list(void);
dd_task_node **get_completed_list(void);
dd_task_node **get_overdue_list(void);

void generator1_callback(TimerHandle_t xTimer);
void generator2_callback(TimerHandle_t xTimer);
void generator3_callback(TimerHandle_t xTimer);

xQueueHandle xQueueMessages;
xQueueHandle xQueueResponses;
BaseType_t dd_scheduler_task;
BaseType_t dd_task_gen1_task;
BaseType_t dd_task_gen2_task;
BaseType_t dd_task_gen3_task;
BaseType_t user_defined_task;
BaseType_t monitor_task;

TimerHandle_t timer_generator1;
TimerHandle_t timer_generator2;
TimerHandle_t timer_generator3;

/* Task IDs */
uint32_t ID1 = 1000;
uint32_t ID2 = 2000;
uint32_t ID3 = 3000;

int main(void)
{
    myDDS_Init();

    /* Start the tasks and timers*/
    xTimerStart(timer_generator1,0);
    xTimerStart(timer_generator2,0);
    xTimerStart(timer_generator3,0);
    vTaskStartScheduler();
    while (1)
    {
    }

    return 0;
}

void myDDS_Init()
{
	/* Initialize Queue*/
	xQueueMessages = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(dd_task_node));
	xQueueResponses = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(dd_task_node));

	if (xQueueMessages == NULL | xQueueResponses == NULL)
	{

		printf("Error creating queues");
	}
	/* Initialize Tasks*/
	dd_scheduler_task = xTaskCreate(dd_scheduler, "dd_scheduler", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH, pxDDS);
	vTaskSuspend(pxDDS);
	monitor_task = xTaskCreate(monitor, "monitor", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);
	dd_task_gen1_task = xTaskCreate(dd_task_generator_1, "dd_task_gen1", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxTaskGen1);
	dd_task_gen2_task = xTaskCreate(dd_task_generator_2, "dd_task_gen2", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxTaskGen2);
	dd_task_gen3_task = xTaskCreate(dd_task_generator_3, "dd_task_gen3", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxTaskGen3);
	user_defined_task = xTaskCreate(user_defined, "user_defined", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxUser);

	if ((dd_scheduler_task == NULL) | (dd_task_gen1_task == NULL) | (dd_task_gen2_task == NULL) | (dd_task_gen3_task == NULL) | (user_defined_task == NULL) | (monitor_task == NULL))
	{
		printf("Error creating tasks");
	}

	/* Timers for each generator using the period for each task */
    timer_generator1 = xTimerCreate("timer1", pdMS_TO_TICKS(t1_period), pdTRUE, 0, generator1_callback);
    timer_generator2 = xTimerCreate("timer2", pdMS_TO_TICKS(t2_period), pdTRUE, 0, generator2_callback);
    timer_generator3 = xTimerCreate("timer3", pdMS_TO_TICKS(t3_period), pdTRUE, 0, generator3_callback);

	printf("dds init\n");
};

void dd_scheduler(void *pvParameters)
{

	dd_task_node *active_list;
	dd_task_node *completed_list;
	dd_task_node *overdue_list;
	dd_message message;
	int period;

	while (1)
	{

		if (xQueueReceive(xQueueMessages, &message, portMAX_DELAY))
		{
			period = get_period(message.task.task_number);

			switch (message.type)
			{
			case release:

				traverse_list(active_list);
				dd_task_node **active_list_head = &active_list;
				message.task.release_time = xTaskGetTickCount();
				message.task.absolute_deadline = xTaskGetTickCount() + period;
				insert_at_back(active_list_head, message.task);
				sort_EDF(active_list_head);
				traverse_list(active_list);

				break;

			case complete:
				break;
			case get_active:
				break;
			case get_completed:
				break;
			case get_overdue:
				break;
			default:
				break;
			}
		}
	}
};
void monitor(void *pvParameters){};
void dd_task_generator_1(void *pvParameters)
{

	while (1)
	{
		printf("gen1");
		release_dd_task(pxTaskGen1, PERIODIC, ++ID1, 1);
		vTaskSuspend(pxTaskGen1);
	}
};

void dd_task_generator_2(void *pvParameters)
{

	while (1)
	{
		printf("gen2");
		release_dd_task(pxTaskGen2, PERIODIC, ++ID2, 2);
		vTaskSuspend(pxTaskGen2);
	}
};
void dd_task_generator_3(void *pvParameters)
{

	while (1)
	{
		printf("gen3");
		release_dd_task(pxTaskGen3, PERIODIC, ++ID3, 3);
		vTaskSuspend(pxTaskGen3);
	}
};

// TODO
void user_defined(void *pvParameters)
{
	dd_task_node *activeList;
	dd_task activeTask;
	uint16_t task_num;
	uint16_t count;

	TickType_t currTick;
	TickType_t prevTick;

	TickType_t executionTick;

	while (1)
	{
		activeList = get_active_list();
		activeTask = activeList->task;
		task_num = activeTask.task_number;
		count = 0;

		switch (task_num)
		{
		case 1:
			executionTick = pdMS_TO_TICKS(t1_execution);
			break;
		case 2:
			executionTick = pdMS_TO_TICKS(t2_execution);
			break;
		case 3:
			executionTick = pdMS_TO_TICKS(t3_execution);
			break;
		default:
			printf("ERROR: could not get task number in user defined task.");
			break;
		}

		currTick = xTaskGetTickCount();
		prevTick = currTick;

		// Will exit once task is complete
		while (count < executionTick)
		{
			currTick = xTaskGetTickCount();
			if (currTick != prevTick)
			{
				count++;
				prevTick = currTick;
			}
		}
		complete_dd_task(activeTask.task_id);
	}
};

/* Core Functionality */

/*
This function receives all of the information necessary to create a new dd_task struct (excluding
the release time and completion time). The struct is packaged as a message and sent to a queue
for the DDS to receive.
*/

/*
	TaskHandle_t t_handle;
	task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
*/
void release_dd_task(TaskHandle_t t_handle, task_type type, uint32_t task_id, uint16_t task_number)
{

	dd_task new_task;
	new_task.t_handle = t_handle;
	new_task.type = type;
	new_task.task_id = task_id;
	new_task.task_number = task_number;

	dd_message new_message;
	new_message.type = release;
	new_message.task = new_task;

	xQueueSendToBack(xQueueMessages, &new_message, portMAX_DELAY);
}

/*
This function receives the ID of the DD-Task which has completed its execution. The ID is packaged
as a message and sent to a queue for the DDS to receive.
*/
void complete_dd_task(uint32_t task_id)
{
	/* delete task from active_list and add to completed */

	dd_task task;
	task.task_id = task_id;

	dd_message new_message;
	new_message.type = complete;
	new_message.task = task;

	xQueueSendToBack(xQueueMessages, &new_message, portMAX_DELAY);
};

/*
This function sends a message to a queue requesting the Active Task List from the DDS. Once a
response is received from the DDS, the function returns the list.
*/
dd_task_node **get_active_list()
{
	dd_task_node **active_list;
	dd_message message;
	message.type = get_active;

	// Send 'get_active' message to DDS
	xQueueSendToBack(xQueueMessages, &message, portMAX_DELAY);

	// Wait for reponse from DDS then return active list
	xQueueReceive(xQueueResponses, &active_list, portMAX_DELAY);

	return active_list;
}

/*
This function sends a message to a queue requesting the Completed Task List from the DDS. Once
a response is received from the DDS, the function returns the list.
*/
dd_task_node **get_completed_list()
{
	dd_task_node **completed_list;
	dd_message message;
	message.type = get_completed;

	// Send 'get_active' message to DDS
	xQueueSendToBack(xQueueMessages, &message, portMAX_DELAY);

	// Wait for reponse from DDS then return completed list
	xQueueReceive(xQueueResponses, &completed_list, portMAX_DELAY);

	return completed_list;
};

/*
This function sends a message to a queue requesting the Overdue Task List from the DDS. Once a
response is received from the DDS, the function returns the list
*/
dd_task_node **get_overdue__list()
{
	dd_task_node **overdue_list;
	dd_message message;
	message.type = get_overdue;

	// Send 'get_active' message to DDS
	xQueueSendToBack(xQueueMessages, &message, portMAX_DELAY);

	// Wait for reponse from DDS then return overdue list
	xQueueReceive(xQueueResponses, &overdue_list, portMAX_DELAY);

	return overdue_list;
};

int get_period(uint16_t task_number)
{

	if (task_number == 1)
	{
		return t1_period;
	}
	else if (task_number == 2)
	{
		return t2_period;
	}
	else if (task_number == 3)
	{
		return t3_period;
	}
	else
		return 0;
}

int get_execution_time(uint16_t task_number)
{

	if (task_number == 1)
	{
		return t1_execution;
	}
	else if (task_number == 2)
	{
		return t2_execution;
	}
	else if (task_number == 3)
	{
		return t3_execution;
	}
	else
		return 0;
}

void generator1_callback(TimerHandle_t xTimer)
{
    vTaskResume(pxTaskGen1);
}

void generator2_callback(TimerHandle_t xTimer)
{
    vTaskResume(pxTaskGen2);
}

void generator3_callback(TimerHandle_t xTimer)
{
    vTaskResume(pxTaskGen3);
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for (;;)
		;
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for (;;)
		;
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if (xFreeStackSpace > 100)
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
/*-----------------------------------------------------------*/
