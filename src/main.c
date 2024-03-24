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

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include "stm32f4_discovery.h"

/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"

#define PRIORITY_HIGH 4
#define PRIORITY_MED 3
#define PRIORITY_LOW 1

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

/* Typedefs */
typedef enum task_type
{
	PERIODIC,
	APERIODIC
} task_type;

// TODO: Extend to include additional info (list of interrupt times ect usefull for debugging Monitor Task)
typedef struct dd_task
{
	TaskHandle_t t_handle;
	task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
} dd_task;

typedef struct dd_task_list
{
	dd_task task;
	struct dd_task_list *next_task;
} list;

/* Prototypes. */
// Task handles ** might not need **
TaskHandle_t pxDDS;
TaskHandle_t pxMonitor;
TaskHandle_t pxUser;
TaskHandle_t pxTaskGen1;
TaskHandle_t pxTaskGen2;
TaskHandle_t pxTaskGen3;

void dd_scheduler(void *pvParameters);
void dd_task_generator_1(void *pvParameters);
void dd_task_generator_2(void *pvParameters);
void dd_task_generator_3(void *pvParameters);
void user_defined(void *pvParameters);
void monitor(void *pvParameters);
void release_dd_task(TaskHandle_t t_handle,
					 task_type type,
					 uint32_t task_id,
					 uint32_t absolute_deadline);

void complete_dd_task(uint32_t task_id);
list **get_active_list(void);
list **get_completed_list(void);
list **get_overdue_list(void);

xQueueHandle xQueueMessages;
BaseType_t dd_scheduler_task;
BaseType_t dd_task_gen1_task;
BaseType_t dd_task_gen2_task;
BaseType_t dd_task_gen3_task;
BaseType_t user_defined_task;
BaseType_t monitor_task;

int main(void)
{

	myDDS_Init();

	/* Start the tasks and timers*/
	vTaskStartScheduler();
	while (1)
	{
	}

	return 0;
}

void myDDS_Init()
{
	/* Initialize Queue*/
	// TODO: check if queuesize of 1 is correct
	xQueueMessages = xQueueCreate(1, sizeof(list));

	if (xQueueMessages == NULL)
	{

		printf("Error creating queues");
		return 0;
	}
	/* Initialize Tasks*/
	dd_scheduler_task = xTaskCreate(dd_scheduler, "dd_scheduler", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH, pxDDS);
	monitor_task = xTaskCreate(monitor, "monitor", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH, NULL);
	dd_task_gen1_task = xTaskCreate(dd_task_generator_1, "dd_task_gen1", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxTaskGen1);
	dd_task_gen2_task = xTaskCreate(dd_task_generator_2, "dd_task_gen2", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxTaskGen2);
	dd_task_gen3_task = xTaskCreate(dd_task_generator_3, "dd_task_gen3", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxTaskGen3);
	user_defined_task = xTaskCreate(user_defined, "user_defined", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, pxUser);

	if ((dd_scheduler_task == NULL) | (dd_task_gen1_task == NULL) | (dd_task_gen2_task == NULL) | (dd_task_gen2_task == NULL) | (user_defined_task == NULL) | (monitor_task == NULL))
	{
		printf("Error creating tasks");
		return 0;
	}
};

void dd_scheduler(void *pvParameters){};
void dd_task_generator1(void *pvParameters){};
void dd_task_generator2(void *pvParameters){};
void dd_task_generator3(void *pvParameters){};
void user_defined(void *pvParameters){};
void monitor(void *pvParameters){};

/* Core Functionality */

/*
This function receives all of the information necessary to create a new dd_task struct (excluding
the release time and completion time). The struct is packaged as a message and sent to a queue
for the DDS to receive.
*/
void release_dd_task(TaskHandle_t t_handle, task_type type, uint32_t task_id, uint32_t absolute_deadline){

};

/*
This function receivesthe ID of the DD-Task which has completed its execution. The ID is packaged
as a message and sent to a queue for the DDS to receive.
*/
void complete_dd_task(uint32_t task_id){
	/* delete task from active_list and add to completed */
};

/*
This function sends a message to a queue requesting the Active Task List from the DDS. Once a
response is received from the DDS, the function returns the list.
*/
list **get_active_list(){};

/*
This function sends a message to a queue requesting the Completed Task List from the DDS. Once
a response is received from the DDS, the function returns the list.
*/
list **get_completed_list(){};

/*
This function sends a message to a queue requesting the Overdue Task List from the DDS. Once a
response is received from the DDS, the function returns the list
*/
list **get_overdue__list(){};

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
