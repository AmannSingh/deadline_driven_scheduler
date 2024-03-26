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

// ms = tick * portTICK_PERIOD_MS

/* Custom includes. */
#include "dd_task_list.h"

#define PRIORITY_HIGH 4
#define PRIORITY_MED 3
#define PRIORITY_LOW 1
#define MESSAGE_QUEUE_SIZE 50
#define MONITOR_PERIOD pdMS_TO_TICKS(2000)

/* TEST BENCH */
#define TEST_BENCH 1
#define HYPER_PERIOD pdMS_TO_TICKS(1500)
/* Set to 1 for additional print statements,
   adds overhead set to 0 and use debugger for final results */
#define PRINT_TEST 1

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
TaskHandle_t pxUser1;
TaskHandle_t pxUser2;
TaskHandle_t pxUser3;
TaskHandle_t pxTaskGen1;
TaskHandle_t pxTaskGen2;
TaskHandle_t pxTaskGen3;

void myDDS_Init();
void results_Init();
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
TickType_t get_period_TICKS(uint16_t task_number);
void print_event(int event_num, int task_num, message_type type, int measured_time);
void move_overdue_tasks(dd_task_node **active_list, dd_task_node **overdue_list);

void complete_dd_task(uint32_t task_id);
dd_task_node *get_active_list(void);
dd_task_node **get_completed_list(void);
dd_task_node **get_overdue_list(void);

void generator1_callback(TimerHandle_t xTimer);
void generator2_callback(TimerHandle_t xTimer);
void generator3_callback(TimerHandle_t xTimer);
void monitor_callback(TimerHandle_t xTimer);

xQueueHandle xQueueMessages;
xQueueHandle xQueueResponses;

BaseType_t dd_scheduler_task;
BaseType_t dd_task_gen1_task;
BaseType_t dd_task_gen2_task;
BaseType_t dd_task_gen3_task;
BaseType_t user_defined_task1;
BaseType_t user_defined_task2;
BaseType_t user_defined_task3;
BaseType_t monitor_task;

TimerHandle_t timer_generator1;
TimerHandle_t timer_generator2;
TimerHandle_t timer_generator3;
TimerHandle_t timer_monitor;

/* Task IDs */
uint32_t ID1 = 1000;
uint32_t ID2 = 2000;
uint32_t ID3 = 3000;

int hyper_period_complete = 0;

int main(void)
{
	myDDS_Init();
	results_Init();

	/* Start the tasks and timers*/
	xTimerStart(timer_generator1, 0);
	xTimerStart(timer_generator2, 0);
	xTimerStart(timer_generator3, 0);
//	xTimerStart(timer_monitor, 0);
	vTaskStartScheduler();
	while (1)
	{
	}

	return 0;
}

void myDDS_Init()
{
	/* Initialize Queue*/
	xQueueMessages = xQueueCreate(MESSAGE_QUEUE_SIZE,  sizeof(dd_message));
	xQueueResponses = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(dd_message));

	if (xQueueMessages == NULL | xQueueResponses == NULL)
	{

		printf("Error creating queues\n");
	}
	/* Initialize Tasks*/
	dd_scheduler_task = xTaskCreate(dd_scheduler, "dd_scheduler", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH, &pxDDS);
	// vTaskSuspend(pxDDS);
	//monitor_task = xTaskCreate(monitor, "monitor", configMINIMAL_STACK_SIZE, NULL, PRIORITY_HIGH, &pxMonitor);
	//vTaskSuspend(pxMonitor);
	dd_task_gen1_task = xTaskCreate(dd_task_generator_1, "dd_task_gen1", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, &pxTaskGen1);
	dd_task_gen2_task = xTaskCreate(dd_task_generator_2, "dd_task_gen2", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, &pxTaskGen2);
	dd_task_gen3_task = xTaskCreate(dd_task_generator_3, "dd_task_gen3", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, &pxTaskGen3);

	user_defined_task1 = xTaskCreate(user_defined, "usr_d1", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, &pxUser1);
	user_defined_task2 = xTaskCreate(user_defined, "usr_d2", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, &pxUser2);
	user_defined_task3 = xTaskCreate(user_defined, "usr_d3", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MED, &pxUser3);
	vTaskSuspend(pxUser1);
	vTaskSuspend(pxUser2);
	vTaskSuspend(pxUser3);

	// if ((dd_scheduler_task == NULL) | (dd_task_gen1_task == NULL) | (dd_task_gen2_task == NULL) | (dd_task_gen3_task == NULL) | (user_defined_task == NULL) | (monitor_task == NULL))
	// {
	// 	printf("Error creating tasks\n");
	// }

	/* Timers for each generator using the period for each task */
	timer_generator1 = xTimerCreate("timer1", pdMS_TO_TICKS(t1_period), pdTRUE, 0, generator1_callback);
	timer_generator2 = xTimerCreate("timer2", pdMS_TO_TICKS(t2_period), pdTRUE, 0, generator2_callback);
	timer_generator3 = xTimerCreate("timer3", pdMS_TO_TICKS(t3_period), pdTRUE, 0, generator3_callback);

	/* Monitor timer. */
//	timer_monitor = xTimerCreate("monitor", MONITOR_PERIOD, pdTRUE, 0, monitor_callback);

	printf("dds init\n");
};

void results_Init()
{
	printf("+-------------------------------------------------------+\n");
	printf("|\tEvent #\t\t\tEvent\t\t\tMeasured Time (ms)\t|\n");
	printf("+-------------------------------------------------------+\n");
}

void dd_scheduler(void *pvParameters)
{

	dd_task_node *active_list = NULL;
	dd_task_node *completed_list = NULL;
	dd_task_node *overdue_list = NULL;

	dd_message message;
	dd_task task;
	TickType_t currTick;
	TickType_t measured_time;
	int period;
	int event_number = 1;

	while (1)
	{
		if (xQueueReceive(xQueueMessages, &message, portMAX_DELAY))
		{
			dd_task_node **active_list_head = &active_list;
			dd_task_node **completed_list_head = &completed_list;

			// checks if any tasks are overdue, if they are move them to the overdue_list and remove from active list
//			move_overdue_tasks(active_list, overdue_list);
			// sort list after checking overdue tasks
//			sort_EDF(active_list_head);
			period = get_period_TICKS(message.task.task_number);

			switch (message.type)
			{
			case release:
				currTick = xTaskGetTickCount();
				measured_time = currTick * portTICK_PERIOD_MS;
				print_event(event_number, message.task.task_number, message.type, measured_time);
				message.task.release_time = currTick;
				message.task.absolute_deadline = currTick + period;
				insert_at_back(active_list_head, message.task);
				sort_EDF(active_list_head);
				set_priority(active_list_head);
				break;

			case complete:
				currTick = xTaskGetTickCount();
				measured_time = currTick * portTICK_PERIOD_MS;
				print_event(event_number, message.task.task_number, message.type, measured_time);
				task = pop(active_list_head);
				sort_EDF(active_list);

				insert_at_back(completed_list_head, task);
				break;

				// check on queue type
			case get_active:
				xQueueSendToBack(xQueueResponses, &active_list, portMAX_DELAY);
				break;

			case get_completed:
				xQueueSendToBack(xQueueResponses, &completed_list, portMAX_DELAY);
				break;

			case get_overdue:
				xQueueSendToBack(xQueueResponses, &overdue_list, portMAX_DELAY);
				break;

			default:
				break;
			}
		}
		if (active_list->task.t_handle != NULL)
		{
			vTaskResume(active_list->task.t_handle);
		}
	}
};
void monitor(void *pvParameters)
{
	dd_task_node *active_list;
	dd_task_node **completed_list;
	dd_task_node **overdue_list;

	int active_count = 0;
	int completed_count = 0;
	int overdue_count = 0;

	while (1)
	{
		active_list = get_active_list();
		completed_list = *get_completed_list();
		overdue_list = *get_overdue_list();

		active_count = get_list_count(active_list);
		completed_count = get_list_count(completed_list);
		overdue_count = get_list_count(overdue_list);

		printf("MONITOR TASK:\n");
		printf("Number of active DD-Tasks: %d\n", active_count);
		printf("Number of completed DD-Tasks: %d\n", completed_count);
		printf("Number of overdue DD-Tasks: %d\n", overdue_count);
		printf("\n\n\n");

		vTaskSuspend(NULL);
	};
};
void dd_task_generator_1(void *pvParameters)
{

	while (1)
	{
		printf("gen1\n");
		release_dd_task(pxUser1, PERIODIC, ++ID1, 1);
		vTaskSuspend(pxTaskGen1);
	}
};

void dd_task_generator_2(void *pvParameters)
{

	while (1)
	{
		printf("gen2\n");
		release_dd_task(pxUser2, PERIODIC, ++ID2, 2);
		vTaskSuspend(pxTaskGen2);
	}
};
void dd_task_generator_3(void *pvParameters)
{

	while (1)
	{
		printf("gen3\n");
		release_dd_task(pxUser3, PERIODIC, ++ID3, 3);
		vTaskSuspend(pxTaskGen3);
	}
};

void user_defined(void *pvParameters)
{
	dd_task_node **activeList;
	dd_task activeTask;
	uint16_t task_num;
	uint16_t count;

	TickType_t currTick;
	TickType_t prevTick;

	TickType_t executionTick;

	dd_message message;

	while (1)
	{
		printf("USER_DEFINED\n");
		activeList = get_active_list();

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
			printf("ERROR: could not get task number in user defined task.\n");
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
		if(activeTask.task_id > 1000 && activeTask.task_id < 4000){
			complete_dd_task(activeTask.task_id);
		}else{
			printf("error: cannot complete task in user defined. NO valid task id\n");
		}

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
	printf("RELEASE_DD_TASK\n");
	dd_task new_task;
	new_task.t_handle = t_handle;
	new_task.type = type;
	new_task.task_id = task_id;
	new_task.task_number = task_number;

	dd_message new_message;
	new_message.type = release;
	new_message.task = new_task;
	new_message.list = NULL;

	xQueueSendToBack(xQueueMessages, &new_message, portMAX_DELAY);
}

/*
This function receives the ID of the DD-Task which has completed its execution. The ID is packaged
as a message and sent to a queue for the DDS to receive.
*/
void complete_dd_task(uint32_t task_id)
{
	/* delete task from active_list and add to completed */
	printf("COMPLETE_DD_TASK\n");
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
dd_task_node *get_active_list()
{
	printf("GET_ACTIVE_LIST\n");
	dd_task_node *active_list;
	dd_message message;
	message.type = get_active;
	message.list = NULL;

	// Send 'get_active' message to DDS
	xQueueSendToBack(xQueueMessages, &message, portMAX_DELAY);

	// Wait for reponse from DDS then return active list
	xQueueReceive(xQueueResponses, &active_list, portMAX_DELAY);

	printf("RETURNING ACTIVE LIST\n");

	return active_list;
}

/*
This function sends a message to a queue requesting the Completed Task List from the DDS. Once
a response is received from the DDS, the function returns the list.
*/
dd_task_node **get_completed_list()
{
	printf("GET_COMPLETED_LIST\n");
	dd_task_node *completed_list;
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
dd_task_node **get_overdue_list()
{
	printf("GET_OVERDUE_LIST\n");
	dd_task_node *overdue_list;
	dd_message message;
	message.type = get_overdue;

	// Send 'get_active' message to DDS
	xQueueSendToBack(xQueueMessages, &message, portMAX_DELAY);

	// Wait for reponse from DDS then return overdue list
	xQueueReceive(xQueueResponses, &overdue_list, portMAX_DELAY);

	return overdue_list;
};

TickType_t get_period_TICKS(uint16_t task_number)
{

	if (task_number == 1)
	{
		return pdMS_TO_TICKS(t1_period);
	}
	else if (task_number == 2)
	{
		return pdMS_TO_TICKS(t2_period);
	}
	else if (task_number == 3)
	{
		return pdMS_TO_TICKS(t3_period);
	}
	else
		return pdMS_TO_TICKS(100);
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

void print_event(int event_num, int task_num, message_type type, int measured_time)
{
	if (measured_time <= HYPER_PERIOD)
	{
		printf("\t%d\t\tTask %d ", event_num, task_num);
		type == release ? printf("released") : printf("completed");
		printf("\t\t\t%d\n", measured_time);
	}
	else if (!hyper_period_complete)
	{
		hyper_period_complete = 1;
		printf("HYPER-PERIOD finished.. \n");
	}
}

void move_overdue_tasks(dd_task_node **active_list, dd_task_node **overdue_list)
{
	dd_task_node *prev = NULL;
	dd_task_node *curr = *active_list;

	while (curr != NULL)
	{
		// Task is overdue TODO: check ticks/ms
		if ((xTaskGetTickCount() + get_period_TICKS(curr->task.task_number)) > curr->task.absolute_deadline)
		{
			// Task is overdue, move it to the overdue_list
			insert_at_back(overdue_list, curr->task);
		}
		else
		{
			// Task is not overdue, move to the next task
			prev = curr;
			curr = curr->next_task;
		}
	}
}

/* Timer callback functions. */
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

void monitor_callback(TimerHandle_t xTimer)
{
	vTaskResume(pxMonitor);
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
