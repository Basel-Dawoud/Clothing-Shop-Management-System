# Clothing Shop Management System - IPC In Linux

This project is a **Clothing Shop Management System** implemented in C. It simulates a clothing shop's inventory management, customer ordering, and admin functionalities. The system uses **shared memory**, **semaphores**, **message queues**, and **signals** for inter-process communication and synchronization. Below is a detailed explanation of the overall flow and the role of each file in the system.

---

## Table of Contents
1. [Overall Flow](#overall-flow)
2. [File Descriptions](#file-descriptions)
   - [shop.h](#shoph)
   - [shopProcess.c](#shopprocessc)
   - [managerProcess.c](#managerprocessc)
   - [customerProcess.c](#customerprocessc)
   - [shop.c](#shopc)
   - [manager.c](#managerc)
   - [customer.c](#customerc)
   - [stock.txt](#stocktxt)
   - [currentStock.txt](#currentstocktxt)
   - [managerPID.txt](#managerpidtxt)
3. [How to Run](#how-to-run)
4. [Future Improvements](#future-improvements)
5. [Contributing](#contributing)
6. [License](#license)

---

## Overall Flow

The system consists of three main processes: **Shop Process**, **Manager Process**, and **Customer Process**. Here's how they interact:

1. **Initialization**:
   - The **Shop Process** (`shopProcess.c`) starts and initializes the system.
   - It reads the initial inventory from `stock.txt` and loads it into **shared memory**.
   - Semaphores (`SHM_SEM_NAME`, `MANAGER_SEM_NAME`, `TERMINAL_SEM_NAME`, `NEXT_ORDER`) are created for synchronization.

2. **Forking Processes**:
   - The **Shop Process** forks two child processes:
     - **Manager Process** (`managerProcess.c`): Handles restocking and stock updates.
     - **Customer Process** (`customerProcess.c`): Handles customer interactions and order placement.

3. **Customer Interaction**:
   - The **Customer Process** displays a menu for the customer to choose between placing an order or accessing admin functionality.
   - If the customer chooses to place an order:
     - The customer selects an item ID and quantity.
     - The order is sent to the **Shop Process** via a **message queue** (`ORDER_QUEUE`).
     - The **Shop Process** checks the item's availability in the shared memory and sends an acknowledgment back to the customer (e.g., "Avail", "Weak", or "NON").
   - If the customer chooses admin functionality:
     - The admin is prompted to enter a password.
     - If authenticated, the admin can restock items or change prices by sending signals to the **Manager Process**.

4. **Stock Management**:
   - The **Shop Process** continuously monitors the stock levels in shared memory.
   - If any item's stock falls below the minimum threshold (`MIN_ITEM_QUANTITY`), the **Shop Process** sends a **signal** (`RESTOCKSIG`) to the **Manager Process** with the item IDs that need restocking.
   - The **Manager Process** restocks the items by updating the shared memory.

5. **Order Processing**:
   - After the customer places an order, the **Shop Process** validates the order and updates the stock levels in shared memory.
   - If the order is valid, the **Shop Process** sends a receipt back to the customer via the message queue.
   - The **Manager Process** is notified to update the stock levels in the shared memory.

6. **Admin Actions**:
   - If the admin chooses to restock or change prices, the **Manager Process** handles the request:
     - For restocking, the manager adds a fixed quantity (e.g., 10 units) to the item's stock.
     - For price changes, the manager updates the item's price in shared memory.

7. **Termination**:
   - The system continues running until all processes are terminated.
   - Shared memory and semaphores are cleaned up, and the final stock levels are saved to `currentStock.txt`.

---

## File Descriptions

### `shop.h`
This is the **header file** that contains all the necessary definitions, structures, and function prototypes used across the project. It includes:
- **Structures**:
  - `StockItem`: Represents an item in the inventory with fields like `itemID`, `category`, `name`, `brand`, `size`, `color`, `price`, `stockQuantity`, and `description`.
  - `customerItem`: Represents a customer order with fields like `itemID`, `quantity`, and `message`.
- **Semaphore Names**: Definitions for semaphores used for synchronization (`SHM_SEM_NAME`, `MANAGER_SEM_NAME`, etc.).
- **Signal Definitions**: Constants for signals like `RESTOCKSIG`, `ADMINSIG`, etc.
- **Function Prototypes**: Declarations for functions used in `shop.c`, `manager.c`, and `customer.c`.

---

### `shopProcess.c`
This is the **main process** that initializes the system. It:
- Reads the initial inventory from `stock.txt` and loads it into shared memory.
- Forks the **Manager Process** and **Customer Process**.
- Monitors stock levels and sends restock signals to the **Manager Process** when necessary.
- Handles customer orders and updates the shared memory.
- Uses **semaphores** to synchronize access to shared resources.

---

### `managerProcess.c`
This process handles **restocking** and **admin actions**. It:
- Listens for signals from the **Shop Process** (e.g., `RESTOCKSIG` for restocking).
- Restocks items by updating the shared memory.
- Handles admin requests (e.g., changing item prices or manually restocking items).
- Uses **semaphores** to ensure exclusive access to shared memory.

---

### `customerProcess.c`
This process handles **customer interactions**. It:
- Displays a menu for customers to place orders or access admin functionality.
- Sends customer orders to the **Shop Process** via a **message queue**.
- Receives acknowledgments and receipts from the **Shop Process**.
- Handles admin authentication and sends admin requests to the **Manager Process**.

---

### `shop.c`
This file contains **helper functions** for the **Shop Process**. It includes:
- Functions to load inventory from `stock.txt` into shared memory.
- Functions to check item availability and process customer orders.
- Functions to print item details and receipts.
- Functions to handle signals and message queues.

---

### `manager.c`
This file contains **signal handlers** and functions for the **Manager Process**. It includes:
- Signal handlers for `RESTOCKSIG` (restocking) and `ADMINSIG` (admin actions).
- Functions to restock items and update stock levels in shared memory.
- Functions to handle admin requests (e.g., changing item prices).

---

### `customer.c`
This file contains **customer-related functions**. It includes:
- Functions to display the customer menu and take orders.
- Functions to handle admin authentication.
- Functions to interact with the **Shop Process** via message queues.

---

### `stock.txt`
This file contains the **initial inventory** of the shop. Each line represents an item with the following format:
```
[Category] [Name] [Brand] [Size] [Color] [Price] [StockQuantity] [Description]
```
Example:
```
T-Shirt T-Shirt Nike M Blue 19.99 50 Comfortable cotton t-shirt
```

---

### `currentStock.txt`
This file stores the **current stock levels** of the shop. It is updated dynamically as customers place orders and items are restocked.

---

### `managerPID.txt`
This file stores the **PID of the Manager Process**. It is used by the **Customer Process** to send signals to the **Manager Process** for restocking or admin actions.

---
Thank you for providing the compilation commands! Based on these, I'll update the **How to Run** section of the README to include the correct compilation steps. Here's the updated section:

---

## How to Run

### Compilation Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/Clothing-Shop-Management-System.git
   cd Clothing-Shop-Management-System
   ```

2. Compile the project using the following commands:
   ```bash
   # Compile shop.c into an object file
   gcc -c shop.c -o shop.o -g -lrt

   # Compile shopProcess.c and link it with shop.o to create the shop executable
   gcc shop.c shopProcess.c -o shop -g -lrt

   # Compile managerProcess.c and manager.c, and link them with shop.o to create the manager executable
   gcc managerProcess.c manager.c shop.o -o manager -g -lrt

   # Compile customer.c and customerProcess.c, and link them with shop.o to create the customer executable
   gcc customer.c customerProcess.c shop.o -o customer -g -lrt
   ```

3. Run the **shop** executable to start the system:
   ```bash
   ./shop
   ```

4. The system will start, and you can interact with it as a customer or admin.

---

### Explanation of Compilation Commands
- **`gcc -c shop.c -o shop.o -g -lrt`**:
  - Compiles `shop.c` into an object file (`shop.o`).
  - The `-g` flag includes debugging information.
  - The `-lrt` flag links the real-time library, which is required for shared memory and semaphores.

- **`gcc shop.c shopProcess.c -o shop -g -lrt`**:
  - Compiles `shop.c` and `shopProcess.c` and links them to create the `shop` executable.
  - This is the main process that initializes the system and forks the manager and customer processes.

- **`gcc managerProcess.c manager.c shop.o -o manager -g -lrt`**:
  - Compiles `managerProcess.c` and `manager.c`, and links them with `shop.o` to create the `manager` executable.
  - This process handles restocking and admin actions.

- **`gcc customer.c customerProcess.c shop.o -o customer -g -lrt`**:
  - Compiles `customer.c` and `customerProcess.c`, and links them with `shop.o` to create the `customer` executable.
  - This process handles customer interactions and order placement.

---

### Running the System
1. Start the **shop** process:
   ```bash
   ./shop
   ```
   - This will initialize the system, load the inventory, and fork the **manager** and **customer** processes.

2. Interact with the system:
   - The **customer** process will allow you to place orders or access admin functionality.
   - The **manager** process will handle restocking and admin requests.

3. Terminate the system:
   - Use `Ctrl+C` to stop the processes.
   - The final stock levels will be saved to `currentStock.txt`.

---

## Future Improvements
- **GUI**: Implement a graphical user interface for easier interaction.
- **Database Integration**: Replace the file-based inventory system with a database for better scalability.
- **Multi-threading**: Handle multiple customers simultaneously using threads.
- **Enhanced Admin Features**: Add more admin functionalities, such as adding or removing items.

---

## Contact

If you have any questions, feedback, or suggestions, feel free to reach out! You can contact me via the following:

Here’s a **Contact** section you can add to your README file. This section provides information on how users or contributors can reach out for questions, feedback, or collaboration.

---

## Contact

If you have any questions, feedback, or suggestions, feel free to reach out! You can contact me via the following:

- **Email**: [Baselinux2024@gmail.com](mailto:Baselinux2024@gmail.com)
- Here’s a **Contact** section you can add to your README file. This section provides information on how users or contributors can reach out for questions, feedback, or collaboration.

---

## Contact

If you have any questions, feedback, or suggestions, feel free to reach out! You can contact me via the following:

- **Email**: [Baselinux2024@gmail.com](mailto:Baselinux2024@gmail.com)  
- **LinkedIn**: [https://www.linkedin.com/in/basel-dawoud/](https://www.linkedin.com/in/basel-dawoud/)  
