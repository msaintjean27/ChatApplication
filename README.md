# ChatApplication — C/GTK Multi-Client Chat

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/msaintjean27/ChatApplication?quickstart=1)

This is a multi-client chatroom application written in C using socket programming and POSIX threads.

# Compile Guidelines

gcc server.c -o server -lpthread
gcc client.c -o client `pkg-config --cflags --libs gtk+-3.0` -pthread

Open 3 terminals, go to project folder, then 
./server -> for 1 terminal
./client <server_ip> <port> -> for 2 or more terminals

Now real-time chat is available between client terminals

# Features
- Multiple clients can connect simultaneously 
- Each client handled on separate threads for concurrency
- Multiple commands supported: /who, /nick <newname>, /msg <user> <text>, /quit. 
	- /who: lists all online users
	- /nick <newname>: Change username
	- /msg <user> <text>: send private message to another user
	- /quit: disconnects you from server
- Broadcasts all messages to each client except the sender
- Allows private messaging between clients
- Chat activity logged in server.log

# Simple Code Explanations
##server.c

socket(), bind(), listen(): Creates the server and prepares it to accept connections.
accept(): Waits for clients to connect.
pthread_create(): Each client gets its own thread to be handled independently.
broadcast(): When one client sends a message, it loops through all connected clients and sends it to them (except the sender).
Handles user commands: /who, /nick <newname>, /msg <user> <text>, /quit
Maintains a client list using MUTEX protection for thread safety
Logs all messages and events to log file, server.log


client.c

socket() + connect(): Connects to the server on 127.0.0.1:8888.
pthread_create(): Receiver thread. This thread keeps listening for messages from the server.
Main thread: Reads user input and sends it to the server.

communication.h

Header file to define shared constants like PORT, BUFFER_SIZE, MAX_CLIENTS
