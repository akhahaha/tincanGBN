# Makefile for tincanGBN

CC = gcc
all: server client
server: server.c
	$(CC) server.c -o server
client: client.c
	$(CC) client.c -o client
clean:
	rm server client *_copy
