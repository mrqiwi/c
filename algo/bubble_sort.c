#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 10

void bubble_sort(int array[], int length);
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

    bubble_sort(array, SIZE);
    puts("");

    puts("Sorted array:");
    for (i = 0; i < SIZE; i++)
        printf("%d ", array[i]);

    return 0;
}

static void swap(int *xp, int *yp)
{
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}

void bubble_sort(int array[], int length)
{
    int i, j;

    for (i = 1; i < length; i++) {

        for (j = 0; j < length-1; j++) {

            if (array[j] > array[j+1])
                swap(&array[j], &array[j+1]);
        }
    }
}
