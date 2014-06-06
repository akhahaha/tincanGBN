/*
	tincanGBN
	===================
	Client-side receiver application.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "packet.c"

void printPkt(struct packet pkt, int io) {
	if (io == 0)
		printf("RECV:\t Type %d\t Seq %d\t Size %d\n",
				pkt.type, pkt.seq, pkt.size);
	else
		printf("SENT:\t Type %d\t Seq %d\t Size %d\n",
				pkt.type, pkt.seq, pkt.size);
	return ;
}

void error(char *msg) {
	perror(msg);
	exit(0);
}

int main(int argc, char* argv[]) {
	int sockfd, portno, curr;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char *hostname, *filename;
	double loss, corrupt;
	socklen_t serv_len;
	struct packet out, in;
	FILE *file;

	// process command line arguments
	if (argc < 6) {
		error("Usage: ./client hostname port filename PLoss PCorruption\n");
		exit(1);
	}
	hostname = argv[1];
	portno = atoi(argv[2]);
	filename = argv[3];
	loss = atof(argv[4]);
	corrupt = atof(argv[5]);
	if (portno < 0)
		error("ERROR port must be greater than 0\n");
	if (loss < 0.0 || loss > 1.0 || corrupt < 0.0 || corrupt > 1.0)
		error("ERROR probabilities must be between 0.0 and 1.0\n");

	// open socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket\n");

	// set server information
	server = gethostbyname(hostname);
	if (server == NULL) {
		error("ERROR no such host\n");
		exit(1);
	}

	serv_len = sizeof(serv_addr);
	bzero((char *) &serv_addr, serv_len);
	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(portno);

	// build request
	printf("Building request for %s\n", filename);
	bzero((char *) &out, sizeof(out));
	out.type = 0;
	out.seq = 0;
	out.size = strlen(filename) + 1;
	strcpy(out.data, filename);

	// send request
	if (sendto(sockfd, &out, sizeof(out), 0,
			(struct sockaddr*) &serv_addr, serv_len) < 0)
		error("ERROR sending request\n");
	printPkt(out, 1);
	printf("----------------------------------------\n");

	curr = 0;
	file = fopen(strcat(filename, "_copy"), "wb");

	bzero((char *) &out, sizeof(out));
	out.type = 2; // ACK packet
	out.seq = curr - 1;
	out.size = 0;

	// gather responses
	while (1) {
		if (recvfrom(sockfd, &in, sizeof(in), 0,
				(struct sockaddr*) &serv_addr, &serv_len) < 0)
			error("ERROR packet lost\n");
		else {
			printPkt(in, 0);

			// only accept next sequential packet
			if (in.seq != curr) {
				printf("IGNORE expected %d\n", curr);
				continue;
			}

			if (in.type == 3) {
				// FIN signal received
				break;
			} else if (in.type != 1) {
				printf("IGNORE not a data packet\n");
				continue;
			}

			fwrite(in.data, 1, in.size, file);
			out.seq = curr;

			if (sendto(sockfd, &out, sizeof(out), 0,
					(struct sockaddr*) &serv_addr, serv_len) < 0)
				error("ERROR sending ACK\n");
			printPkt(out, 1);

			curr++;
		}
	}

	// send FIN ACK
	bzero((char *) &out, sizeof(out));
	out.type = 3; // FIN packet
	out.seq = curr;
	out.size = 0;
	if (sendto(sockfd, &out, sizeof(out), 0,
			(struct sockaddr*) &serv_addr, serv_len) < 0)
		error("ERROR sending FIN\n");
	printPkt(out, 1);

	printf("Exiting client\n");
	fclose(file);
	close(sockfd);
	return 0;
}
