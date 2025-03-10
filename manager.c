/* Signal Handlers for the coming signals to the Manager Process */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "shop.h"

#define SHOP_NUM 1
#define ADMIN_NUM 2
#define UPDATE_NUM 3

char sigType = 0;

void restock_items(int *weakItemsIDs)
{
    int i = 0, j = 0;
    int itemID = weakItemsIDs[j];
    while (itemID != -1)
    { // last element = -1
        for (i = 0; i < itemsCount; i++)
        {
            if (allItems[i].itemID == itemID)
            {
                // Restock Item
                allItems[i].stockQuantity = 10; // Restock to 10 units
                break;
            }
        }
        itemID = weakItemsIDs[++j];
    }
}

// Signal Handler for the Shop Process
void restock_signal_handler(int signum, siginfo_t *info, void *ptr)
{
    sem_t *shm_semaphore = sem_open(SHM_SEM_NAME, 0);

    // Check the signal number
    if (signum == RESTOCKSIG && info->si_value.sival_ptr != NULL)
    {
        sigType = SHOP_NUM;
        // Handle the weak items IDs sent with the signal
        int *weakItemsIDs = (int *)info->si_value.sival_ptr;

        // Acquire semaphore
        // Hold the shared memory semaphore to prevent other processes from accessing the shared memory
        if (sem_wait(shm_semaphore) == -1)
        {
            perror("sem_wait");
            return;
        }

        // Process the weak items IDs as needed (restock the items)
        restock_items(weakItemsIDs);
    }
    // clean up semaphores at the end of the program
    sem_close(shm_semaphore);
}

void update_stock(customerItem **orders)
{
    for (int i = 0; i < MAX_ORDERS; i++)
    {
        if (orders[i] == NULL)
        {
            break;
        }
        int itemID = orders[i]->itemID;
        int quantity = orders[i]->quantity;
        for (int j = 0; j < MAX_ITEMS; j++)
        {
            if (allItems[j].itemID == itemID)
            {
                // Update Stock Quantity
                allItems[j].stockQuantity -= quantity;
                break;
            }
        }
    }
}

// Signal Handler for the customer process (Update Stock Request)
void update_signal_handler(int signum, siginfo_t *info, void *ptr)
{
    sem_t *shm_semaphore = sem_open(SHM_SEM_NAME, 0);

    // Check the signal number
    if (signum == UPDATE_NUM)
    {
        sigType = UPDATE_NUM;
        // Handle the updated stock information sent with the signal
        customerItem **orders = (customerItem **)info->si_value.sival_ptr;

        // Acquire semaphore
        // Hold the shared memory semaphore to prevent other processes from accessing the shared memory
        if (sem_wait(shm_semaphore) == -1)
        {
            perror("sem_wait");
            return;
        }

        // Process the orders as needed (update the stock quantities)
        update_stock(orders);
    }
    // clean up semaphores at the end of the program
    sem_close(shm_semaphore);
}

// Signal Handler for the Customer Process (Admin Request)
void admin_signal_handler(int signum)
{
    // printf("adminHandler\n");
    // Check the signal number
    if (signum == ADMINSIG)
    {
        sigType = ADMIN_NUM;
    }
}

// Function to set up signal handlers
void setup_signal_handlers()
{
    struct sigaction sa;

    // Setup restock signal handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = restock_signal_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(RESTOCKSIG, &sa, NULL) == -1)
    {
        perror("sigaction");
    }

    // Setup update signal handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = update_signal_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(UPDATE_SIG, &sa, NULL) == -1)
    {
        perror("sigaction");
    }

    // Set up the signal handler
   if (signal(ADMINSIG, admin_signal_handler) == SIG_ERR)
   {
        perror("Error Setting Signal Handler");
   }
}

/*
// Function to check the type of the signal
int checkSignalType(siginfo_t *info)
{
    // Check the signal number
    if (info->si_signo == SIGRTMIN)
    {
        // Signal from the Shop Process
        return 1;
    }
    else if (info->si_signo == SIGRTMIN + 1)
    {
        // Signal from the Customer Process
        return 2;
    }
    else
    {
        // Unknown signal
        return 0;
    }
}
*/