#include <stdio.h>
#include <stdlib.h>

typedef struct stack_node {
    int data;
    struct stack_node *next_ptr;
} stack_t;


void push(stack_t **top_ptr, int info);
int pop(stack_t **top_ptr);
void printStack(stack_t *current_ptr);

int main(void)
{
    stack_t *stack_ptr = NULL;

    push(&stack_ptr, 5);
    push(&stack_ptr, 8);
    push(&stack_ptr, 10);

    printStack(stack_ptr);

    pop(&stack_ptr);

    printStack(stack_ptr);

    return 0;
}

void push(stack_t **top_ptr, int num)
{
    stack_t *new_ptr;

    new_ptr = malloc(sizeof(stack_t));

    if (new_ptr == NULL) {
        printf("%d not inserted. No memory available.\n", num);
        return;
    }

    new_ptr->data = num;
    new_ptr->next_ptr = *top_ptr;
    *top_ptr = new_ptr;
}

int pop(stack_t **top_ptr)
{
    stack_t *temp_ptr;
    int pop_palue;

    temp_ptr = *top_ptr;
    pop_palue = (*top_ptr)->data;
    *top_ptr = (*top_ptr)->next_ptr;
    free(temp_ptr);

    return pop_palue;
}

void printStack(stack_t *current_ptr)
{
    if (current_ptr == NULL) {
        printf("The stack is empty.\n\n");
    } else {
        printf("The stack is:\n");

        while (current_ptr != NULL) {
            printf("%d --> ", current_ptr->data);
            current_ptr = current_ptr->next_ptr;
        }

        printf("NULL\n\n");
    }

}
