# Chat Application

# Introduction:
- This app can be either server or client, means it can connect to other apps, and it also can accept new connections from other apps.
- The maximum connections is 2 by default, you can change it by changing the value of "MAX_CONNECTIONS" macro in the main.c.

# Testing video:
https://www.youtube.com/watch?v=AazIQK-3z7Y

# How to use this app:
- Compile & delete it by using the makefile (compile: make; delete: make clean)
- Execute the app:  ./chat  'port-number'  (port-number is the port that you want this app will listen on)
- Type the commands below to control the app:
    1.  help                             : display user interface options
    2.  myip                             : display IP address of this app
    3.  myport                           : display listening port of this app
    4.  connect 'destination' 'port no'  : connect to the app of another computer
    5.  list                             : list all the connections of this app
    6.  terminate 'connection id'        : terminate a connection
    7.  send 'connection id' 'message'   : send a message to a connection;
    8.  exit                             : close all connections & terminate this app 
