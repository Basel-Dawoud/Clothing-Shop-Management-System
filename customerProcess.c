#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "shop.h"


sem_t *shm_semaphore;
sem_t *manager_semaphore;
sem_t *terminal_semaphore;
sem_t *nextCustomer_sem;

#define MAX_MSG_SIZE 256

customerItem *itemSelected;
customerItem *orders[MAX_ORDERS];

int adminAuthentication();
void handle_error(const char *msg);

void handle_error(const char *msg)
{
    perror(msg);
    sem_post(terminal_semaphore);
    exit(EXIT_FAILURE);
}

int main()
{
    const char *MP_PID = "/home/basel/Documents/clothingShop/managerPID.txt";
    int mpfd = open(MP_PID, O_RDONLY);
    if (mpfd == -1)
    {
        handle_error("Error opening the MP file for reading");
    }

    // Read the manager PID from the file
    int managerPid;
    if (fscanf(fdopen(mpfd, "r"), "%d", &managerPid) != 1)
    {
        handle_error("Error reading the manager PID");
        close(mpfd);
        return 1;
    }
    close(mpfd);

    // Open the semaphores
    shm_semaphore = sem_open(SHM_SEM_NAME, 0);
    manager_semaphore = sem_open(MANAGER_SEM_NAME, 0);
    terminal_semaphore = sem_open(TERMINAL_SEM_NAME, 0);
    nextCustomer_sem = sem_open(NEXT_ORDER, 0);

    if (shm_semaphore == SEM_FAILED || manager_semaphore == SEM_FAILED || terminal_semaphore == SEM_FAILED || nextCustomer_sem == SEM_FAILED)
    {
        handle_error("Error opening the semaphore");
    }

    // Wait for the next customer
    sem_wait(nextCustomer_sem);

    while (1)
    {
        printf("Customer Process Starts..\n");

        // Ask for service selection
        printf("Choose the Service:\n");
        printf("1. Customer\n");
        printf("2. Admin\n");
        int service;
        scanf("%d", &service);

        if (service == 1)
        {
            // Customer service logic
            printf("Customer Service..\n");
            mqd_t mq;
            char mq_message[MAX_MSG_SIZE];
            // Create message queue
            struct mq_attr attr = {
                .mq_maxmsg = 10,    // Number of messages in the queue
                .mq_msgsize = 4096  
            };
            // Open the message queue for sending messages
            mqd_t fdOrderQ = mq_open(ORDER_QUEUE, O_CREAT | O_RDWR, 0666, &attr);
            if (fdOrderQ == (mqd_t)-1)
            {
                handle_error("Error opening the message queue");
            }

           // snprintf(message, sizeof(message), "Message %d", i + 1);

            union sigval step;
            step.sival_int = 1;

            char ack[10];
            ssize_t msgLen;
            char orderAgain = 'Y';
            int ordersCount = 0;

            while (orderAgain == 'Y' && ordersCount < MAX_ORDERS)
            {
                // Display menu and get customer order
                itemSelected = printCustomerMenu();

                // Notify shop to start receiving orders
                if (sigqueue(getppid(), MQ_SIG, step) == -1)
                {
                    handle_error("Error sending the signal to the manager process");
                }

                // Send the order to the manager process
                int itemID = itemSelected->itemID;
                if (mq_send(fdOrderQ, "itemID", strlen("itemID"), 0) == -1)
                {
                    handle_error("Error sending the message to the manager process");
                }

                // Wait for acknowledgment from the shop
                msgLen = mq_receive(fdOrderQ, ack, sizeof(ack), 0);
                if (msgLen == -1)
                {
                    handle_error("Error Receiving the Acknowledgment");
                }

                // Check if item is available in stock
                if (strcmp(ack, "Avail") == 0)
                {
                    orders[ordersCount] = itemSelected;
                    ordersCount++;
                }
                else if (strcmp(ack, "Weak") == 0)
                {
                    printf("Order is out of stock! In stock quantity for item with ID: %d is: %s\n", itemSelected->itemID, ack);
                }
                else
                {
                    printf("No Item with that ID, Please Enter a valid ID...\n");
                }

                // Ask the customer whether to order again
                while (1)
                {
                    printf("Order Again? (Y/N): ");
                    scanf(" %c", &orderAgain);
                    if (orderAgain == 'N' || orderAgain == 'Y')
                    {
                        break;
                    }
                    else
                    {
                        printf("Invalid Input. Please Enter Y or N.\n");
                    }
                }
            }

            // Send signal to validate the entire order list
            step.sival_int = 2;
            if (sigqueue(getppid(), MQ_SIG, step) == -1)
            {
                handle_error("Error sending the signal to the manager process");
            }

            // Send the receipt request and orders to the shop
            strcpy(itemSelected->message, "Receipt");
            itemSelected->quantity = ordersCount;
            itemSelected->itemID = 0;

            if (mq_send(fdOrderQ, (char *)itemSelected, sizeof(customerItem), 0) == -1)
            {
                handle_error("Error sending the receipt request");
            }

            // Send the order array to the shop
            if (mq_send(fdOrderQ, (char *)orders, sizeof(orders), 0) == -1)
            {
                handle_error("Error sending the orders");
            }

            // Wait for acknowledgment of the order validation
            msgLen = mq_receive(fdOrderQ, ack, sizeof(ack), 0);
            if (msgLen == -1)
            {
                handle_error("Error Receiving the Acknowledgment");
            }

            if (strcmp(ack, "Avail") == 0)
            {
                printf("Items are available and order will be processed.\n");
            }
            else
            {
                printf("Items quantities are too high than available. Please try again.\n");
                sleep(1);
            }

            // Send the orders to the manager to update the stock
            sem_wait(manager_semaphore);

            union sigval allOrders;
            allOrders.sival_ptr = orders;

            // Send a signal to the manager with the orders
            if (sigqueue(managerPid, RESTOCKSIG, allOrders) == -1)
            {
                handle_error("Error sending the signal to the manager process");
            }

            // Release the semaphore
            sem_post(manager_semaphore);

            // Cleanup: close message queue and unlink
            mq_close(fdOrderQ);
            mq_unlink(ORDER_QUEUE);

            // Allow next customer process to proceed
            sleep(10);
        }
        else
        {
            // Admin Authentication logic
            if (adminAuthentication())
            {
                printf("Admin Authenticated. Sending signal to manager...\n");
                kill(managerPid, ADMINSIG);
                sem_wait(manager_semaphore);

                sleep(3);
                kill(getppid(), NEXT_ORDER_SIG);
                sem_post(nextCustomer_sem);
                sleep(1);

                sem_wait(nextCustomer_sem);
            }
            else
            {
                printf("Authentication Failed!\n");
                sleep(2);
            }
        }
    }

    return 0;
}
