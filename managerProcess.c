/* Manager Process */

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "shop.h"

#define SHOP_NUM 1
#define ADMIN_NUM 2
#define UPDATE_NUM 3

extern char sigType;

void restock_items(int *weakItemsIDs);
void update_stock(customerItem **orders);

sem_t *shm_semaphore;
sem_t *manager_semaphore;
sem_t *terminal_semaphore;

void setup_signal_handlers();

int main()
{
    // Setting the signals handlers for the manager process
    setup_signal_handlers();

    // Wait for a signal from a process
    pause();
    printf("\nManager starts\n");
    fflush(NULL);

    shm_semaphore = sem_open(SHM_SEM_NAME, 0);
    manager_semaphore = sem_open(MANAGER_SEM_NAME, 0);
    terminal_semaphore = sem_open(TERMINAL_SEM_NAME, 0);

    if (shm_semaphore == SEM_FAILED || manager_semaphore == SEM_FAILED || terminal_semaphore == SEM_FAILED)
    {
        perror("Error opening the semaphore");
        exit(1);
    }

    const char *outputFile = "/home/basel/Documents/clothingShop/currentStock.txt";

    int fdOutput = open(outputFile, O_RDWR | S_IRUSR | S_IWUSR);
    if (fdOutput == -1)
    {
        perror("Error opening the output file");
        return 1;
    }

    //  itemsCount = 7;

    size_t fileSize = getFileSize(fdOutput);
    if (fileSize == (size_t)-1)
    {
        close(fdOutput);
        return 1;
    }
    /*
        // Map the output file in memory to be shared between the child process (Manager)
        mappedData = (char *)mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fdOutput, 0);
        if (mappedData == MAP_FAILED)
        {
            perror("Error mapping the file in memory");
            close(fdOutput);
            return 1;
        }

        // Load all items in the inventory
        allItems = (StockItem *)mmap(NULL, (sizeof(StockItem) * itemsCount), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, fdOutput, 0);
        if (allItems == MAP_FAILED)
        {
            perror("Error mapping the file in memory");
            close(fdOutput);
            return 1;
        }


        // Load all items in the inventory
        for (int i = 0; i < itemsCount; i++)
        {
            loadInventory(mappedData, &allItems[i], i + 1, fileSize);
        }
    */
    /*
        for (int i = 0; i < itemsCount; i++)
        {
            printItemData(&allItems[i]);
        }
    */

    // Get the file name for shared memory from environment variable
    char *filename = getenv("SHARED_MEMORY");
    if (filename == NULL)
    {
        perror("Shared memory file not found");
        exit(1);
    }

    // Open the shared memory file
    int fd = open(filename, O_RDWR);
    if (fd == -1)
    {
        perror("Error opening shared memory file");
        exit(1);
    }

    // Get the size of the shared memory file
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("Error getting the file size");
        exit(1);
    }
    itemsCount = sb.st_size / sizeof(StockItem);

    // Map the shared memory file to memory
    allItems = (StockItem *)mmap(NULL, sizeof(StockItem) * itemsCount, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (allItems == MAP_FAILED)
    {
        perror("Error mapping memory");
        exit(1);
    }

    while (1)
    {
        // Check the type of the signal
        if (sigType == SHOP_NUM) // If the signal is from the shop process
        {
            // Restock the items in the shared memory

            // Release the shared memory semaphore to allow other processes to access the shared memory
            sem_post(shm_semaphore);

            // Wait for the next signal...
        }
        else // If the signal is from the customer process
        {
            // sem_wait(terminal_semaphore);

            // Checking whether the signal is for Admin or Stock Update
            if (sigType == ADMIN_NUM) // If the signal is for Admin => terminal fg
            {
                // Hold the terminal semaphore to prevent other processes from writing to the terminal
                // sem_wait(terminal_semaphore);

                // Get the item ID from the customer to be managed
                int itemID;
                printf("Enter the item ID to be managed: ");
                scanf("%d", &itemID);

                // Asking the user to choose the action to be done on the item (Restock the Item or Changing its Price)
                int action;
                printf("Choose the action to be done on the item:\n");
                printf("1. Restock the Item\n");
                printf("2. Change the Item Price\n");
                printf("Enter the action number: ");
                scanf("%d", &action);

                // Perform the action based on the user's choice
                if (action == 1) // If the user chose to restock the item
                {
                    // Restock the item in the shared memory
                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (allItems[i].itemID == itemID)
                        {
                            allItems[i].stockQuantity += 10; // Restock by adding 10 units
                            printf("Restocked..\n");
                            if (msync(allItems, sizeof(StockItem) * itemsCount, MS_SYNC) == -1)
                            {
                                perror("Error syncing the mapped file");
                            }
                            fflush(NULL);
                            break;
                        }
                    }

                    // Wait for the next signal...
                }
                else // If the user chose to change the item price
                {
                    // Change the item price in the shared memory
                    float newPrice;
                    printf("Enter the new price for the item: ");
                    scanf("%f", &newPrice);

                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (allItems[i].itemID == itemID)
                        {
                            allItems[i].price = newPrice;
                            break;
                        }
                    }

                    // Wait for the next signal...
                }

                // updating the mapped file

                sem_post(manager_semaphore);
            }
            else // If the signal is for Stock Update
            {
                while (1)
                {
                    // Get the item ID from the user to be restocked
                    int itemID;
                    printf("Enter the item ID to restock: ");
                    scanf("%d", &itemID);

                    // Get the quantity to restock the item with
                    int quantity;
                    printf("Enter the quantity to restock the item with: ");
                    scanf("%d", &quantity);

                    // Restock the item in the shared memory
                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (allItems[i].itemID == itemID)
                        {
                            printf("RRRRRRRRRRRRRRRRRRRR");
                            allItems[i].stockQuantity = quantity;
                            break;
                        }
                    }

                    // Ask if the user wants to restock another item
                    char choice;
                    printf("Do you want to restock another item? (y/n): ");
                    scanf(" %c", &choice);

                    if (choice != 'y' && choice != 'Y')
                    {
                        break;
                    }
                }
            }
            // Release the terminal semaphore to allow other processes to write to the terminal
            // sem_post(terminal_semaphore);

            // Release the shared memory semaphore to allow other processes to access the shared memory
            // sem_post(shm_semaphore);

            // Wait for the next signal...
        }
        pause();

        // Write the contents of the shared memory to the output file
        if (write(fdOutput, allItems, sizeof(StockItem) * itemsCount) == -1)
        {
            perror("Error writing to output file");
            close(fdOutput);
            exit(1);
        }

        printf("Manager process saved the shared memory to currentStock.txt\n");
    }

    // Clean up
    close(fdOutput);
    // Clean up
    munmap(allItems, sizeof(StockItem) * itemsCount);
    close(fd);
    remove("shared_memory.tmp");

    return 0;
}
