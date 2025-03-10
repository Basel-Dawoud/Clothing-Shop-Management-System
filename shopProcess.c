#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>
#include <aio.h>

#include "shop.h"

sem_t *shm_semaphore;
sem_t *manager_semaphore;
sem_t *terminal_semaphore;
sem_t *nextCustomer_sem;

extern char signalType;
extern char **environ;  // Declare environ globally

int main()
{
    const char *inputFile = "/home/basel/Documents/clothingShop/stock.txt";
    const char *outputFile = "/home/basel/Documents/clothingShop/currentStock.txt";
    const char *MP_PID = "/home/basel/Documents/clothingShop/managerPID.txt";
    const char *managerProcess = "/home/basel/Documents/clothingShop/manager";
    const char *customerProcess = "/home/basel/Documents/clothingShop/customer";

    // Defining semaphores
    shm_semaphore = sem_open(SHM_SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    manager_semaphore = sem_open(MANAGER_SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    terminal_semaphore = sem_open(TERMINAL_SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    nextCustomer_sem = sem_open(NEXT_ORDER, O_CREAT | O_EXCL, 0666, 1);

    if (shm_semaphore == SEM_FAILED || manager_semaphore == SEM_FAILED || terminal_semaphore == SEM_FAILED)
    {
        perror("Error initializing the semaphore");
        sem_unlink(SHM_SEM_NAME);
        sem_unlink(MANAGER_SEM_NAME);
        sem_unlink(TERMINAL_SEM_NAME);
        sem_unlink(NEXT_ORDER);
        exit(1);
    }

    // Open the inventory file for reading
    int fdInput = open(inputFile, O_RDONLY);
    if (fdInput == -1)
    {
        perror("Error opening the input file");
        return 1;
    }

    size_t fileSize = getFileSize(fdInput);
    if (fileSize == (size_t)-1)
    {
        close(fdInput);
        return 1;
    }

    /* Mapping the input file in a shared memory */
    mappedData = (char *)mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fdInput, 0);
    if (mappedData == MAP_FAILED)
    {
        perror("Error mapping the file in memory");
        close(fdInput);
        return 1;
    }
    // Create the output file for managing data
    int fdOutput = open(outputFile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fdOutput == -1)
    {
        perror("Error opening the output file");
        munmap(mappedData, fileSize);
        close(fdInput);
        return 1;
    }

    if (ftruncate(fdOutput, fileSize) == -1)
    {
        perror("Error truncating the output file");
        close(fdInput);
        close(fdOutput);
        munmap(mappedData, fileSize);
        return 1;
    }

    /*
    // Print the first 50 characters to verify the mapped data
    printf("Mapped Data Preview: %.50s\n", mappedData);
    */

    // Count the number of items and categories
    itemsCount = itemsNum(mappedData);
    // catCount = catNum(mappedData);

    // Write the mapped data (the stock file) to a new file for processing
    ssize_t bytesWritten = write(fdOutput, mappedData, fileSize);
    if (bytesWritten == -1)
    {
        perror("Error writing to the output file");
        close(fdInput);
        close(fdOutput);
        munmap(mappedData, fileSize);
        return 1;
    }

    // Unmap and close the initialization file after use
    if (munmap(mappedData, fileSize) == -1)
    {
        perror("Error unmapping the file");
        close(fdInput);
        return 1;
    }
    // Close the initializaion file
    close(fdInput);

    // Map the output file in memory to be shared between the child process (Manager)
    mappedData = (char *)mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fdOutput, 0);
    if (mappedData == MAP_FAILED)
    {
        perror("Error mapping the file in memory");
        close(fdOutput);
        return 1;
    }

    char printWelcome[] = "\nWelcome to Basel's Shop!\n\n";
    for (int i = 0; i < strlen(printWelcome); i++)
    {
        printf("%c", printWelcome[i]);
        fflush(stdout); // Ensure each character is printed immediately
        //  usleep(100000); // Sleep for 100 milliseconds
    }

    // Create a file-backed shared memory region
    int fd = open("shared_memory.tmp", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }

    // Set the size of the file (the size of shared memory)
    if (ftruncate(fd, sizeof(StockItem) * itemsCount) == -1) {
        perror("Error setting file size");
        exit(1);
    }

    // Load all items in the inventory
    allItems = (StockItem *)mmap(NULL, (sizeof(StockItem) * itemsCount), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (allItems == MAP_FAILED)
    {
        perror("Error mapping the file in memory");
        close(fdOutput);
        return 1;
    }

    // Set the shared memory address in the environment variable for the child
    if (setenv("SHARED_MEMORY", "shared_memory.tmp", 1) != 0) {
        perror("Failed to set environment variable");
        exit(1);
    }

    // Load all items in the inventory
    for (int i = 0; i < itemsCount; i++)
    {
        loadInventory(mappedData, &allItems[i], i + 1, fileSize);
    }
    
    /*
        // Unmap and close the initialization file after use
        if (munmap(mappedData, fileSize) == -1)
        {
            perror("Error unmapping the file");
            close(fdInput);
            return 1;
        }
      */
    // Close the initializaion file
    close(fdInput);

    /*
    StockItem *categories = (StockItem *)malloc(sizeof(StockItem) * catCount);
    for (int i = 0; i <= catCount; i++)
    {
        printCar(&categories[i]);
    }
    */

    /*
        printf("Items: %ld\n", itemsCount);
        printf("Categories: %ld\n", catCount);
    */

    /*
    StockItem item1;
    loadInventory(mappedData, &item1, 3, fileSize);
    printItemData(&item1);
    */

    /*
        // Write the mapped data (the stock file) to a new file for processing
        ssize_t bytesWritten = write(fdOutput, mappedData, fileSize);
        if (bytesWritten == -1)
        {
            perror("Error writing to the output file");
            close(fdInput);
            close(fdOutput);
            munmap(mappedData, fileSize);
            return 1;
        }
    */

    // Forking Children Processes

    // Fork the first child process (Manager)
    managerPid = fork();

    if (managerPid == 0)
    {
        // Manager Process - this will be replaced by execve()
        char *args[] = {managerProcess, NULL};  // Arguments to pass to execve
        execve(args[0], args, environ);  // Pass environment to execve
        perror("execve failed");
        exit(1);
/*
        // Running the manager process
        if (execl(managerProcess, "managerProcess", (char *)NULL) == -1)
        {
            perror("Error executing the manager process");
            exit(1); // Exit with error code
        }
  */
    }
    else if (managerPid < 0)
    {
        perror("Error forking the manager process");
        return 1;
    }
    else
    {
        // Parent Process (Shop Process)

        int mpfd = open(MP_PID, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
        // Create the output file for managing data
        if (mpfd == -1)
        {
            perror("Error opening the MP file");
            return 1;
        }

        // Write the manager PID into the file
        dprintf(mpfd, "%d\n", managerPid);

        // Close the file
        close(mpfd);

        // Configure the real-time signal represents the MQ
        struct sigevent sev;
        struct sigaction sa;
        // Set up the signal handler using sigaction
        memset(&sa, 0, sizeof(sa));
        sa.sa_flags = SA_SIGINFO;     // Use SA_SIGINFO to receive extra data
        sa.sa_sigaction = mq_handler; // Set handler with sa_sigaction
        sigemptyset(&sa.sa_mask);     // No signal mask
        if (sigaction(MQ_SIG, &sa, NULL) == -1)
        {
            perror("sigaction");
            exit(1);
        }

        // Set up the signal handler
        if (signal(NEXT_ORDER_SIG, nextOrder_signal_handler) == SIG_ERR)
        {
            perror("Error Setting Signal Handler");
        }

        // Hold the terminal semaphore to prevent other processes from accessing the terminal while printing
        sem_wait(nextCustomer_sem);

        // Fork the second child process (Customer)
        pid_t customerPid = fork();

        if (customerPid == 0)
        {
            // Customer Process
            // printf("Customer Process Forked\n");

            // Running the customer process
            if (execl(customerProcess, "customerProcess", (char *)NULL) == -1)
            {
                perror("Error executing the customer process");
                exit(1); // Exit with error code
            }
        }
        else if (customerPid < 0)
        {
            perror("Error forking the customer process");
            return 1;
        }
        else
        {
            // Declaring a container for the IDs of the weak items
            int weakItems[itemsCount];
            int weakItemsCount = 0, childrenExitStatus = 0, status = 0;

            while (1)
            {
                // Wait a short time to check children exit status
                for (int i = 0; i < 2; i++)
                {
                    childrenExitStatus = waitpid(-1, &status, WNOHANG); // Check for any child process that has finished
                    if (childrenExitStatus > 0)                         //  A child has exited
                    {
                        if (childrenExitStatus == managerPid)
                        {
                            printf("Manager Process Exited\n");
                        }
                        else if (childrenExitStatus == customerPid)
                        {
                            printf("Customer Process Exited\n");
                        }
                    }
                    sleep(1);
                }

                // No more children left, break the loop
                if (childrenExitStatus < 0)
                {
                    break;
                }


                // Checking the Items Quantities in the mapped memory
                for (int i = 0; i < itemsCount; i++)
                {
                    if (allItems[i].stockQuantity < MIN_ITEM_QUANTITY)
                    {
                        // Add the item ID to the weak items container
                        weakItems[weakItemsCount] = allItems[i].itemID;
                        weakItemsCount++;
                    }
                }

                // Check if there are weak items
                if (weakItemsCount)
                {
                    for (int i = 0; i < itemsCount; i++)
                    {
                        printItemData(&allItems[i]);
                        //  sleep(1);
                    }
                    sleep(1);

                    printf("\nWEAK\n");
                    // Restocking...
                    // Some Items are out of stock => Signal the Manager Process and send the weak items IDs with it to restock them

                    // Release the shared memory semaphore to allow the manager process to access the shared memory
                    sem_post(shm_semaphore);

                    // Hold the manager semaphore to prevent Customer Process from signalling the manager process right now
                    // sem_wait(manager_semaphore);
                    // printf("Shop Aquired ms");

                    weakItems[weakItemsCount] = -1;
                    union sigval weakItemsIDs;
                    weakItemsIDs.sival_ptr = weakItems; // Send the pointer to the weak items IDs array

                    // Send a real-time signal (SIGRTMIN) along with the data
                    if (sigqueue(managerPid, RESTOCKSIG, weakItemsIDs) == -1)
                    {
                        perror("Error sending the signal to the manager process");
                        return 1;
                    }
                    sleep(1); // Wait for the manager process to handle the weak items

                    // Wait for the shm_semaphore to be released by the manager process
                    sem_wait(shm_semaphore);

                    // Release the manager semaphore to allow the customer process to signal the manager process
                    // sem_post(manager_semaphore);

                    // Check the Items Quantities in the shared memory again...
                }
                else
                {
                    /* Loading & Printing Available Items In the shared memory to the Customer */
                    char print[] = "Available Items...\n\n";
                    for (int i = 0; i < strlen(print); i++)
                    {
                        printf("%c", print[i]);
                        fflush(stdout); // Ensure each character is printed immediately
                        //  usleep(100000); // Sleep for 100 milliseconds
                    }

                    // All Items Quantities are good
                    for (int i = 0; i < itemsCount; i++)
                    {
                        // loadInventory(mappedData, &allItems[i], i + 1, fileSize);
                        printItemData(&allItems[i]);
                        //  sleep(1);
                    }

                    sem_post(nextCustomer_sem);

                    while (signalType != 1)
                    {
                        pause();
                    }
                    signalType = 0;

                    printf("Passed the pause in the shopProcess");
                    fflush(NULL);
                    /*
                    printf("Shop posts the terminalSem");

                    // Release the terminal semaphore to allow other processes to access the terminal
                    sem_post(terminal_semaphore);

                    // Release the shared memory semaphore to allow the manager process to access the shared memory
                    sem_post(shm_semaphore);

                    //printf("Shop pauses");
                    //fflush(NULL);
                    */
                    // Wait for the signal from the customer process indicating that the current customer has finished
                    sem_wait(nextCustomer_sem);

                    // Hold the shared memory semaphore to prevent the manager process from accessing the shared memory while accessing it
                    // sem_wait(shm_semaphore);
                }
            }
        }
    }

    // clean up semaphores at the end of the program
    sem_close(shm_semaphore);
    sem_close(manager_semaphore);
    sem_close(terminal_semaphore);

    sem_unlink(SHM_SEM_NAME);
    sem_unlink(MANAGER_SEM_NAME);
    sem_unlink(TERMINAL_SEM_NAME);

    // unmap and close the output file after use
    if (munmap(mappedData, fileSize) == -1)
    {
        perror("Error unmapping the file");
        close(fdOutput);
        return 1;
    }

    // Clean up and unmap the memory
    if (munmap(allItems, sizeof(StockItem) * itemsCount) == -1)
    {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    // Close the output file
    close(fdOutput);

    return 0;
}
