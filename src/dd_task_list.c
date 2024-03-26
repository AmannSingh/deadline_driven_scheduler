#include <dd_task_list.h>

void insert_at_front(dd_task_node **head, dd_task new_task)
{
    dd_task_node *new_node = (dd_task_node *)malloc(sizeof(dd_task_node));
    if (new_node == NULL)
    {
        printf("Memory allocation failed.\n");
        return;
    }
    new_node->task = new_task;
    new_node->next_task = *head;
    *head = new_node;
}

void insert_at_back(dd_task_node **head, dd_task new_task)
{
    dd_task_node *new_node = (dd_task_node *)malloc(sizeof(dd_task_node));
    dd_task_node *last = *head;

    new_node->task = new_task;
    new_node->next_task = NULL;

    if (*head == NULL)
    {
        *head = new_node;
        return;
    }

    while (last->next_task != NULL)
    {
        last = last->next_task;
    }
    last->next_task = new_node;
}

// return entire node instead of task

dd_task pop(dd_task_node **head)
{
    dd_task task;
    if (*head == NULL)
    {
        printf("List is empty.\n");
        return;
    }
    dd_task_node *temp = *head;
    task = temp->task;
    *head = (*head)->next_task;
    free(temp);
    return task;
}
/* Sort by absolute deadline, using bubble sort. */
void sort_EDF(dd_task_node **head)
{
    int is_swapped;
    dd_task_node *current;
    dd_task_node *last_sorted = NULL;

    if (*head == NULL)
        return;

    do
    {
        is_swapped = 0;
        current = *head;

        while (current->next_task != last_sorted)
        {
            if (current->task.absolute_deadline > current->next_task->task.absolute_deadline)
            {
                // Swap tasks
                dd_task temp_task = current->task;
                current->task = current->next_task->task;
                current->next_task->task = temp_task;
                is_swapped = 1;
            }
            current = current->next_task;
        }
        last_sorted = current;
    } while (is_swapped);
}

void set_priority(dd_task_node **head){
	dd_task_node* current = *head;
	vTaskPrioritySet(current->task.t_handle, PRIORITY_MED);

	current = current->next_task;
	while(current!=NULL)
	{
		vTaskPrioritySet(current->task.t_handle, PRIORITY_LOW);
	}

}
int get_list_count(dd_task_node *head)
{
    int count = 0;
    dd_task_node *current = head;

    while (current != NULL)
    {
        count++;
        current = current->next_task; // Move to the next node
    }
    return count;
}

void delete_node_by_task_id(dd_task_node **head, uint32_t task_id) {
    dd_task_node *temp = *head, *prev = NULL;

    // If the head node itself holds the task to be deleted
    if (temp != NULL && temp->task.task_id == task_id) {
        *head = temp->next_task; // Changed head
        free(temp); // free old head
        return;
    }

    // Search for the task to be deleted, keep track of the previous node
    // as we need to change 'prev->next'
    while (temp != NULL && temp->task.task_id != task_id) {
        prev = temp;
        temp = temp->next_task;
    }

    // If task_id was not present in the list
    if (temp == NULL) return;

    // Unlink the node from the linked list
    prev->next_task = temp->next_task;

    free(temp); // Free memory
}

dd_task_node* create_empty_list(){

    dd_task_node *empty_list = (dd_task_node*)malloc(sizeof(dd_task_node));

    empty_list->task.absolute_deadline = 0;
    empty_list->task.completion_time = 0;
    empty_list->task.release_time = 0;
    empty_list->task.t_handle = NULL;
    empty_list->task.task_id = 0;
    empty_list->task.task_number = 0;
    empty_list->task.type = 0;

    empty_list->next_task = NULL;

    return empty_list;
}


