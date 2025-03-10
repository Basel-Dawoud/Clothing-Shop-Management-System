/* Customer Functions */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>    /* For O_* constants */
#include <sys/stat.h> /* For mode constants */
#include <mqueue.h>
#include <string.h>

#include "shop.h"

#pragma pack(push, 1)
// Function to print the customer menu and take the order
customerItem *printCustomerMenu()
{
    customerItem *item = (customerItem *)malloc(sizeof(customerItem));
    if (item == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    printf("\n\n");
    printf("Place your order...\n");

    // Asking for the Item ID and Quantity the customer wants to buy
    printf("Enter the Item ID: ");
    scanf("%d", &item->itemID);
    printf("Enter the Quantity: ");
    scanf("%d", &item->quantity);

    return item;
}
#pragma pack(pop)

// Function for Admin Authentication with 3 trials
int adminAuthentication()
{
    // Admin Password
    char password[] = "admin";
    char input[20];
    int trials = 3;

    // Loop for 3 trials
    while (trials > 0)
    {
        printf("Enter the Admin Password: ");
        scanf("%s", input);

        // Check the password
        if (strcmp(input, password) == 0)
        {
            return 1;
        }
        else
        {
            printf("Wrong Password! %d Trials Left\n", trials - 1);
            trials--;
        }
    }

    return 0;
}
