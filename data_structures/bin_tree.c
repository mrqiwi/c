#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct tree_node {
    int data;
    struct tree_node *left_ptr;
    struct tree_node *right_ptr;
} tree_t;


void insert_node(tree_t **tree_ptr, int value);
void in_order(tree_t *tree_ptr);
void pre_order(tree_t *tree_ptr);
void post_order(tree_t *tree_ptr);

int main(void)
{
    int item;
    tree_t *root_ptr = NULL;

    srand(time(NULL));
    printf("The numbers being placed in the tree are:\n");

    for (int i = 1; i <= 10; i++) {
        item = rand() % 15;
        printf("%3d", item);
        insert_node(&root_ptr, item);
    }

    printf("\n\nThe preorder traversal is:\n");
    pre_order(root_ptr);

    printf("\n\nThe inorder traversal is:\n");
    in_order(root_ptr);

    printf("\n\nThe postorder traversal is:\n");
    post_order(root_ptr);

    return 0;
}

void insert_node(tree_t **tree_ptr, int value)
{
    if (*tree_ptr == NULL) {
        *tree_ptr = malloc(sizeof(tree_t));

        if (*tree_ptr != NULL) {
            (*tree_ptr)->data = value;
            (*tree_ptr)->left_ptr = NULL;
            (*tree_ptr)->right_ptr = NULL;
        } else {
            printf("%d not inserted. No memory available.\n", value);
        }

    } else {

        if (value < (*tree_ptr)->data)
            insert_node(&((*tree_ptr)->left_ptr), value);
        else if (value > (*tree_ptr)->data)
            insert_node(&((*tree_ptr)->right_ptr), value);
        else
            printf("(dup)");
    }
}
// симметричный обход
void in_order(tree_t *tree_ptr)
{
    if (tree_ptr != NULL) {
        in_order(tree_ptr->left_ptr);
        printf("%3d", tree_ptr->data);
        in_order(tree_ptr->right_ptr);
    }
}

// обход в прямом направлении
void pre_order(tree_t *tree_ptr)
{
    if (tree_ptr != NULL) {
        printf("%3d", tree_ptr->data);
        pre_order(tree_ptr->left_ptr);
        pre_order(tree_ptr->right_ptr);
    }
}

// обход в обратном направлении
void post_order(tree_t *tree_ptr)
{
    if (tree_ptr != NULL) {
        post_order(tree_ptr->left_ptr);
        post_order(tree_ptr->right_ptr);
        printf("%3d", tree_ptr->data);
    }
}
