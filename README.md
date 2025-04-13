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
  - “num_requests” - The number requests that each client thread will send.<br />
# Analysis <br />
