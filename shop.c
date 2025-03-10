/* Shop helper functions & handlers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>    /* For O_* constants */
#include <sys/stat.h> /* For mode constants */
#include <mqueue.h>
#include <signal.h>
#include <semaphore.h>

#define MAX_ORDERS 10

#include "shop.h"

customerItem itemReceived; // Static buffer
customerItem Orders[MAX_ORDERS];
StockItem *allItems = NULL;

long itemsCount = 0;
long catCount = 0;
char *mappedData;
pid_t managerPid;

char signalType = 0;

// Signal Handler for the Customer Process (Next Order Request)
void nextOrder_signal_handler(int signum)
{
    // printf("NextOrderHandler\n");
    // Check the signal number
    if (signum == NEXT_ORDER_SIG)
    {
        signalType = 1;
    }
}

/* Handler for the message queue notification */
void mq_handler(int signum, siginfo_t *info, void *context)
{
    if (signum == MQ_SIG)
    {
        printf("Received MQ_SIG signal\n");
        fflush(stdout);
    }

    // Open the semaphore for synchronization
    sem_t *shm_semaphore = sem_open(SHM_SEM_NAME, 0);
    if (shm_semaphore == SEM_FAILED)
    {
        perror("sem_open failed");
        return;
    }

    /*
        // Access the data passed with the notification
        char *notif_data = info->si_value.sival_ptr;

        // Process the message queue
        mqd_t mqdes = (mqd_t)info->si_value.sival_int; // Get the message queue descriptor from the signal value
    */

    // OR
    // Open the existing message queue created by the customer process
    mqd_t mqdes = mq_open(ORDER_QUEUE, O_RDWR);
    if (mqdes == (mqd_t)-1)
    {
        perror("mq_open failed");
        sem_close(shm_semaphore);
        return;
    }
    char message[100];
    ssize_t bytes_received;

    // Parse the request type from the signal value
    int request = info->si_value.sival_int;
    if (request == 1)
    {
        printf("sizeof(customerItem): %ld\n",sizeof(customerItem));
        fflush(stdout);

        sleep(1);
        // Request type 1: Handle customer order
        char id[100];
        bytes_received = mq_receive(mqdes, (char *)&id, strlen(id), NULL);
        if (bytes_received == -1)
        {
            perror("E");
            mq_close(mqdes);
            sem_close(shm_semaphore);
            return;
        }

        // Process the received order
        sem_wait(shm_semaphore);

        int availQ = getItemQuant(itemReceived.itemID);

        if (availQ != -1)
        {
            if (availQ >= itemReceived.quantity)
            {
                // Send ACK => Available
                strcpy(message, "Avail");
                if (mq_send(mqdes, message, strlen(message) + 1, 0) == -1)
                {
                    perror("Error sending ACK: Avail");
                    sem_post(shm_semaphore);
                    mq_close(mqdes);
                    return;
                }
            }
            else
            {
                // Send ACK => Weak (not enough stock)
                strcpy(message, "Weak");
                if (mq_send(mqdes, message, strlen(message) + 1, 0) == -1)
                {
                    perror("Error sending ACK: Weak");
                    sem_post(shm_semaphore);
                    mq_close(mqdes);
                    return;
                }
            }
        }
        else
        {
            // No Item with that ID
            strcpy(message, "NON");
            if (mq_send(mqdes, message, strlen(message) + 1, 0) == -1)
            {
                perror("Error sending ACK: NON");
                sem_post(shm_semaphore);
                mq_close(mqdes);
                return;
            }
        }

        sem_post(shm_semaphore);
    }
    else if (request == 2)
    {
        // Request type 2: Check availability of all orders
        customerItem orders[MAX_ORDERS];
        bytes_received = mq_receive(mqdes, (char *)orders, sizeof(orders), NULL);
        if (bytes_received == -1)
        {
            perror("Error receiving orders");
            mq_close(mqdes);
            sem_close(shm_semaphore);
            return;
        }

        // Process the order availability check
        // Group orders by itemID and check total quantities
        for (int i = 0; i < MAX_ORDERS; i++)
        {
            if (orders[i].quantity > 0) // Only check items that have quantity > 0
            {
                // Sum quantities for the same itemID
                int totalQuantity = orders[i].quantity;
                for (int j = i + 1; j < MAX_ORDERS; j++)
                {
                    if (orders[j].itemID == orders[i].itemID)
                    {
                        totalQuantity += orders[j].quantity;
                        // Set the quantity of the duplicate item to 0 to avoid double-counting
                        orders[j].quantity = 0;
                    }
                }
                // Now check the total quantity available in stock for this itemID
                int availQ = getItemQuant(orders[i].itemID);
                if (availQ == -1 || availQ < totalQuantity)
                {
                    // If not enough stock, send a "Quantities Too High" message
                    strcpy(message, "Quantities Too High");
                    if (mq_send(mqdes, message, strlen(message) + 1, 0) == -1)
                    {
                        perror("Error sending availability response");
                        mq_close(mqdes);
                        sem_close(shm_semaphore);
                        return;
                    }
                    sem_post(shm_semaphore);
                    return;
                }
            }
        }

        // All orders available
        strcpy(message, "All Available");
        if (mq_send(mqdes, message, strlen(message) + 1, 0) == -1)
        {
            perror("Error sending availability response");
            sem_post(shm_semaphore);
            mq_close(mqdes);
            return;
        }
        sem_post(shm_semaphore);
    }

    // Check if the received request is for the "Recipt"
    else if (strcpy(itemReceived.message, "Receipt"))
    {
        // Request type 3: Receipt request
        customerItem orders[MAX_ORDERS];
        bytes_received = mq_receive(mqdes, (char *)orders, sizeof(orders), NULL);
        if (bytes_received == -1)
        {
            perror("Error receiving orders for receipt");
            mq_close(mqdes);
            sem_close(shm_semaphore);
            return;
        }

        // Print and send the receipt
        printReceipt(orders);

        sem_post(shm_semaphore);
    }
    else
    {
        fprintf(stderr, "Unknown request type received: %s\n", message);
    }
    // Cleanup
    mq_close(mqdes);
    sem_close(shm_semaphore);
}

// Function to get the item quantity in the stock
int getItemQuant(int itemID)
{
    for (int i = 0; i < itemsCount; i++)
    {
        if (allItems[i].itemID == itemID)
        {
            return allItems[i].stockQuantity;
        }
    }
    return -1;
}

// Function to print the receipt
void printReceipt(customerItem orders[MAX_ORDERS])
{
    double totalCost = 0.0;
    int itemNumber;

    printf("Receipt:\n");
    printf("----------------------------\n");
    for (int i = 0; i < MAX_ORDERS; i++)
    {
        itemNumber = orders[i].itemID - 100;
        if (orders[i].quantity > 0)
        {
            double itemCost = orders[i].quantity * allItems[itemNumber].price;
            totalCost += itemCost;
            printf("Item: %s\n", allItems[itemNumber].name);
            printf("Quantity: %d\n", orders[i].quantity);
            printf("Price: %.2f\n", allItems[itemNumber].price);
            printf("Cost: %.2f\n", itemCost);
            printf("----------------------------\n");
        }
    }
    printf("Total Cost: %.2f\n", totalCost);
    printf("----------------------------\n");

    // Send the receipt to the customer process using the message queue
    mqd_t mqdes = mq_open(ORDER_QUEUE, O_WRONLY);
    if (mqdes == (mqd_t)-1)
    {
        perror("mq_open");
        return;
    }

    char receipt[1024];
    snprintf(receipt, sizeof(receipt), "Receipt:\nTotal Cost: %.2f\n", totalCost);
    if (mq_send(mqdes, receipt, strlen(receipt) + 1, 0) == -1)
    {
        perror("mq_send");
    }

    mq_close(mqdes);
}

int catNum(char *mappedData)
{
    /* Extracting Number of Categories */
    char ch = '\0', index = 0, carr[CAT_DIGITS];
    int catCount = 0;

    while (ch != '(')
    {
        ch = (mappedData)[index++];
    }
    ch = (mappedData)[index];
    for (char j = 0; ch != ')'; j++)
    {
        carr[j] = ch;
        ch = (mappedData)[++index];
    }
    for (char i = 0; i < CAT_DIGITS; i++)
    {
        catCount = catCount * 10 + (carr[i] - '0');
    }
    return catCount;
}

int itemsNum(char *mappedData)
{
    /* Extracting Number of Items */
    char ch = '\0', i = 0, index = 0, iarr[ITEMS_DIGITS];
    int itemsCount = 0;

    while (i < 2)
    {
        ch = (mappedData)[index++];
        if (ch == '(')
        {
            i++;
        }
    }
    ch = (mappedData)[index];
    for (char j = 0; ch != ')'; j++)
    {
        iarr[j] = ch;
        ch = (mappedData)[++index];
    }
    for (char i = 0; i < ITEMS_DIGITS; i++)
    {
        itemsCount = itemsCount * 10 + (iarr[i] - '0');
    }
    return itemsCount;
}

size_t GetFileSize(int fd)
{
    /* Calculating the file size using lseek */
    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1)
    {
        perror("Error getting the file size");
        close(fd);
        return 1;
    }
    lseek(fd, 0, SEEK_SET); // Reset the file pointer to the beginning of the file
    return fileSize;

    /* OR: Using fstat with stat structure */
    /*
        struct stat fileStat;
        if (fstat(fd, &fileStat) == -1) {
            perror("Failed to get file stats");
            close(fd);
            return;
        }
        size_t fileSize = fileStat.st_size;
    */
}

size_t getFileSize(int fd)
{
    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        perror("Error getting the file size");
        return -1;
    }
    // printf("File Size: %ld\n", st.st_size);
    return st.st_size;
}

void loadInventory(char *mappedData, StockItem *item, int itemNum, size_t fileSize)
{
    // printf("Loading inventory for item %d\n", itemNum);

    // Mapping a writable buffer for tokenization
    char *dataCopy = (char *)mmap(NULL, (fileSize + 1), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (dataCopy == MAP_FAILED)
    {
        perror("Error allocating memory for data copy");
        exit(1);
    }
    dataCopy[fileSize] = '\0'; // Null-terminate the copied data

    // Copy the mapped data to the writable buffer
    if (memcpy(dataCopy, mappedData, fileSize) == NULL)
    {
        perror("Error copying data to the buffer");
        if (munmap(dataCopy, fileSize + 1) == -1)
        {
            perror("munmap");
        }
        exit(1);
    }

    // Print the first 100 characters for diagnostic purposes
    // printf("File content preview: %.100s\n", dataCopy);

    int itemID = 1000 + itemNum; // Starting ItemID
    item->itemID = itemID;

    // Parse the mapped data line by line
    char *line_start = dataCopy;
    char line[1024];
    for (char lineCount = 0; lineCount < itemNum; lineCount++)
    {
        // Find the next newline character
        char *line_end = strchr(line_start, '\n');

        // If there's no newline, this is the last line
        if (line_end == NULL)
        {
            line_end = line_start + strlen(line_start); // End of the remaining data
        }
        else
        {
            // Calculate the length of the current line
            size_t line_length = line_end - line_start + 1;

            // Copy the current line into the 'line' buffer
            memcpy(line, line_start, line_length);
            line[line_length] = '\0'; // Null-terminate the string

            // Skip comment lines (starting with #) & empty lines (only newline)
            while (line[0] == '#' || line[0] == '\n')
            {
                // Move to the next line
                line_start = line_end + 1;
                line_end = strchr(line_start, '\n');

                // If there's no newline, we've reached the end of the data
                if (line_end == NULL)
                {
                    line_end = line_start + strlen(line_start);
                }

                // Copy the next line into the 'line' buffer
                line_length = line_end - line_start + 1;
                memcpy(line, line_start, line_length);
                line[line_length] = '\0'; // Null-terminate the string
            }
            // Move to the next line
            line_start = line_end + 1;
        }
    }
    char *token = strtok(line, "\n"); // Tokenization with "\n"
    // Tokenize the line into fields
    int tokenCount = 0;
    token = strtok(line, " \t"); // Tokenize by space or tab
    while (token != NULL)
    {
        // Process each token
        if (tokenCount == 0)
        {
            strcpy(item->category, token); // Category
        }
        else if (tokenCount == 1)
        {
            strcpy(item->name, token); // Name
        }
        else if (tokenCount == 2)
        {
            strcpy(item->brand, token); // Brand
        }
        else if (tokenCount == 3)
        {
            strcpy(item->size, token); // Size
        }
        else if (tokenCount == 4)
        {
            strcpy(item->color, token); // Color
        }
        else if (tokenCount == 5)
        {
            item->price = atof(token); // Price
        }
        else if (tokenCount == 6)
        {
            item->stockQuantity = atoi(token); // StockQuantity
        }
        else
        {
            // Join all remaining tokens as the description
            if (tokenCount == 7)
            {
                strcpy(item->description, token); // Start description
            }
            else
            {
                strcat(item->description, " ");   // Append a space to the end
                strcat(item->description, token); // Add description part
            }
        }
        token = strtok(NULL, " \t"); // Get next token
        tokenCount++;
    }

    // unmap the file after use
    if (munmap(dataCopy, (fileSize + 1)) == -1)
    {
        perror("Error unmapping the file");
        close(1);
    }
}

void printItemData(StockItem *item)
{
    // Print the item fields
    printf("ItemID: %d\n", item->itemID);
    printf("Category: %s\n", item->category);
    printf("Name: %s\n", item->name);
    printf("Brand: %s\n", item->brand);
    printf("Size: %s\n", item->size);
    printf("Color: %s\n", item->color);
    printf("Price: %.2f\n", item->price);
    printf("Stock Quantity: %d\n", item->stockQuantity);
    printf("Description: %s\n", item->description);
    printf("----------------------------\n");
}

// Checking Available Items In the current Stock
int restockCheck(StockItem *allItems, int itemsCount)
{
    for (int i = 0; i < itemsCount; i++)
    {
        if (allItems[i].stockQuantity < MIN_ITEM_QUANTITY)
        {
            printf("Item %s is running out of stock.\nRestocking ...\n", allItems[i].name);
            printf("\n");
            /* Sending a signal to the Manager process to restock the Item */
            return 1;
        }
    }
    return 0;
}
