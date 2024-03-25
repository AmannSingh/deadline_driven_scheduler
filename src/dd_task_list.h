
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4_discovery.h"
#include "../FreeRTOS_Source/include/task.h"

void insert_at_front(dd_task_node **head, dd_task new_task);
void insert_at_back(dd_task_node **head, dd_task new_task);
void delete_at_front(dd_task_node **head);
dd_task pop(dd_task_node **head);
void sort_EDF(dd_task_node **head);