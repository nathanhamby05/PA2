# How to Compile and Run Task 1
Command Line to Compile Executable: <br />
```gcc -o pa2_binary_task1 task1.c -pthread``` <br />
Command Line to Start Server First:<br />
```./pa2_binary_task1 server 127.0.0.1 12345```<br />
Command Line to Start Client:<br />
```./pa2_binary_task1 client 127.0.0.1 12345 4 1000000```<br />
Acceptable Command Line Arguments:<br />
  - “<server|client>” – decide whether the skeleton code will run as a server or as a client<br />
program (see example usage below)<br />
  - “server_ip” - The IP address of the server.<br />
  - “server_port” - The port number of the server.<br />
  - “num_client_threads” - The number of client threads to launch.<br />
  - “num_requests” - The number requests that each client thread will send.<br />

# How to Compile and Run Task 2 <br />
Command Line to Compile Executable: <br />
```gcc -o pa2_binary_task2 task2.c -pthread```<br />
Command Line to Start Server First:<br />
```./pa2_binary_task2 server 127.0.0.1 12345```<br />
Command Line to Start Client:<br />
```./pa2_binary_task2 client 127.0.0.1 12345 4 1000000```<br />
Acceptable Command Line Arguments:<br />
  - “<server|client>” – decide whether the skeleton code will run as a server or as a client<br />
program (see example usage below)<br />
  - “server_ip” - The IP address of the server.<br />
  - “server_port” - The port number of the server.<br />
  - “num_client_threads” - The number of client threads to launch.<br />
  - “num_requests” - The number of requests that each client thread will send.<br />
# Analysis <br />
### Task 1
  When running ```./pa2_binary_task1 client 127.0.0.1 12345 4 1000000``` the results show that now packet loss is exhibited, but when adding more threads such as ```./pa2_binary_task1 client 127.0.0.1 12345 250 100``` packet loss can be noted. This shows that the UDP handling of the packets is overloading the server and losing minor packets. The more packets that are added, the more loss can be seen. When testing this, I recommend not using a high num_request for the sake of time, sending millions of requests.

### Task 2
  When running the command listed above that had packet loss for Task 1, my task2.c file effectively using retransmission to avoid the packet loss as can be seen by using ```./pa2_binary_task2 client 127.0.0.1 12345 250 100```. This can be observed by raising the number of threads and seeing no packet loss.
