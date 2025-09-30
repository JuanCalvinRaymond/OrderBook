Simple Order Book With 4 Commands:

- Adding Order
- Cancel Order
- View Order List
- View Order Book

<img width="617" height="197" alt="OrderBook Main Menu" src="https://github.com/user-attachments/assets/82383215-694f-400e-9956-a360d15d5b72" />

Enter the corresponding number to execute.

# Build
Project is setup with task.json to build inside vscode or cmake.

To manually build use:
`g++ src/main.cpp -o out/main -std=c++20`

# Adding Order
<img width="617" height="197" alt="OrderBook Add Order" src="https://github.com/user-attachments/assets/e0da2f21-12fa-4948-b400-687b0a3361c4" />

To add an order use the format:

Order type, Side, Price, Quantity

ex. `1(GoodTillCancel) 1(Buy) 200 3` `1(GoodTillCancel) 2(Sell) 150 3`

# Cancel Order
<img width="617" height="197" alt="OrderBook Cancel Order" src="https://github.com/user-attachments/assets/07a9b743-e43c-4400-aa14-0d1b33f2a839" />

To cancel an order enter the orderId which starts at 1 and increment with order that is added.

# View Order List
<img width="617" height="197" alt="OrderBook OrderList" src="https://github.com/user-attachments/assets/365443f2-f849-4328-b19b-f58b70bcb0f6" />

View active order entry history to find order id to be use to cancel an order.

# View Order Book
<img width="617" height="197" alt="OrderBook" src="https://github.com/user-attachments/assets/678d54c6-cba9-453c-a78d-49ae3994aae6" />

View order book will show the total quantity of order that needs to be fulfilled.
