#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 10

void selection_sort(int array[], int length);
static void swap(int *xp, int *yp);

int main(void)
{
    int i, array[SIZE];

    srand(time(NULL));
    for (i = 0; i < SIZE; i++)
        array[i] = rand() % 90 + 10;

    puts("Unsorted array:");
    for (i = 0; i < SIZE; i++)
        printf("%d ", array[i]);

    selection_sort(array, SIZE);
    puts("");

    puts("Sorted array:");
    for (i = 0; i < SIZE; i++)
        printf("%d ", array[i]);

    return 0;
}

void selection_sort(int array[], int length)
{
    int i, j;
    int smallest;

    for (i = 0; i < length - 1; i++) {
        smallest = i;

        for (j = i + 1; j < length; j++)
            if (array[j] < array[smallest])
                smallest = j;

        swap(&array[i], &array[smallest]);
    }
}

static void swap(int *xp, int *yp)
{
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}
