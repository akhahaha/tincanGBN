tincanGBN
===================
	UCLA CS 118 Lab 2
	Professor Mario Gerla
	Spring 2014

	Alan Kha		904030522	akhahaha@gmail.com
-------------------------------------------------------------------------------
Summary
---------------
A reliable data transfer protocol using UDP sockets and Go-Back-N protocol.

Features
---------------
 - Simulated packet loss and corruption.
 - Persistent server

Setup
---------------
1. Run '$ make' to compile client.c and server.c.
2. Start server with '$ ./server portno win_size PLoss PCorrupt'.
3. Start client with '$ ./client hostname portno filename PLoss PCorrupt'.
