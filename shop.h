/* Header file of the program */

#ifndef SHOP_H
#define SHOP_H

#include <signal.h> // For siginfo_t
#include <stdlib.h>
#include <semaphore.h>

#define SHM_SEM_NAME "/shm_sem"
#define MANAGER_SEM_NAME "/manager_sem"
#define TERMINAL_SEM_NAME "/terminal_sem"
#define NEXT_ORDER "/nextOrder_sem"

#define MAX_ITEMS 20 // Max number of items in the inventory
#define CAT_DIGITS 3
#define ITEMS_DIGITS 3
#define MIN_ITEM_QUANTITY 5
#define ORDER_QUEUE "/shopCustomerMQ"
#define MAX_ORDERS 10
#define RECEIPT_SIZE 256

/* Signals Used in the program */
#define ADMINSIG SIGCONT
#define NEXT_ORDER_SIG SIGCONT
#define RESTOCKSIG SIGRTMIN
#define MQ_SIG (SIGRTMIN + 1)
#define UPDATE_SIG (SIGRTMIN + 2)

typedef struct
{
    int itemID;
    char category[50];
    char name[50];
    char brand[50];
    char size[20];
    char color[20];
    double price;
    int stockQuantity;
    char description[200];
} StockItem;

// Structure to hold the customer item
typedef struct
{
    int itemID;
    int quantity;
    char message[10]; // Assuming a fixed size for the message
} customerItem;


// Constants for the Number of Items and Categories in the stock file
extern long itemsCount;
extern long catCount;

// Declare global variables to be accessed by all other files including this file
extern char *mappedData;
extern pid_t managerPid;
extern StockItem *allItems;

// Function declarations

void semInit();

void nextOrder_signal_handler(int signum);

void mq_handler(int signum, siginfo_t *info, void *context);

int getItemQuant(int itemID);

void printReceipt(customerItem orders[MAX_ORDERS]);

int catNum(char *mappedData);

int itemsNum(char *mappedData);

size_t getFileSize(int fd);

size_t GetFileSize(int fd);

void loadInventory(char *mappedData, StockItem *item, int itemNum, size_t fileSize);

void printItemData(StockItem *item);

/* for customer process */
customerItem *printCustomerMenu();

#endif // SHOP_H
