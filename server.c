/*
	tincanGBN
	===================
	Server-side sender application.
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "packet.c"

int simFault(double prob) {
	if ((double) rand() / (double) RAND_MAX < (double) prob)
		return 1;

	return 0;
}

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
	int sockfd, portno, win_size, total, curr, seq_base;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	double loss, corrupt;
	struct packet in, out;
	char *filename;
	FILE *file;

	// process command line arguments
	if (argc < 5)
		error("Usage: ./server port window_size PLoss PCorruption\n");
	portno = atoi(argv[1]);
	win_size = atoi(argv[2]);
	loss = atof(argv[3]);
	corrupt = atof(argv[4]);
	if (portno < 0)
		error("ERROR port must be greater than 0\n");
	if (win_size < 0)
		error("ERROR window size must be at least 1\n");
	if (loss < 0.0 || loss > 1.0 || corrupt < 0.0 || corrupt > 1.0)
		error("ERROR probabilities must be between 0.0 and 1.0\n");

	// setup sockets
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket\n");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	// allocate port to socket
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding\n");

	listen(sockfd, 5);
	clilen = sizeof(cli_addr);

	fd_set readset;
	struct timeval timeout = {1, 0}; // 1 sec timeout

	srand(time(NULL));

	while (1) {
		// wait for file request
		printf("Waiting for file request\n");
		if (recvfrom(sockfd, &in, sizeof(in), 0,
				(struct sockaddr*) &cli_addr, &clilen) < 0) {
			printf("Packet lost\n");
			continue;
		}
		printPkt(in, 0);

		// open requested file
		if (in.type != 0) {
			printf("IGNORE not request packet\n");
			continue;
		}

		filename = in.data;
		printf("File requested is \"%s\"\n", filename);
		file = fopen(filename, "rb");
		if (file == NULL) {
			perror("ERROR");
			printf("Sending abort FIN signal\n");
			bzero((char *) &out, sizeof(out));
			out.type = 3; // FIN packet
			out.seq = 0;
			out.size = 0;
			if (sendto(sockfd, &out, sizeof(out), 0,
					(struct sockaddr*) &cli_addr, clilen) < 0)
				error("ERROR sending abort FIN\n");
			printPkt(out, 1);
			continue;
		}

		// calculate required packets
		struct stat st;
		stat(filename, &st);
		total = st.st_size / PACKET_SIZE;
		if (st.st_size % PACKET_SIZE > 0)
			total++;
		printf("Required packets: %d\n", total);
		printf("----------------------------------------\n");

		// initialize GBN state
		curr = 0;
		seq_base = 0;

		// start GBN procedure
		while (seq_base < total) {
			FD_ZERO(&readset);
			FD_SET(sockfd, &readset);
			if (select(sockfd+1, &readset, NULL, NULL, &timeout) < 0) {
				error("ERROR on select\n");
			} else if (FD_ISSET(sockfd, &readset)) {
				if (recvfrom(sockfd, &in, sizeof(in), 0,
						(struct sockaddr*) &cli_addr, &clilen) < 0) {
					printf("Packet lost");
					curr = 0;
					continue;
				}

				// simulate loss and corruption
				if (simFault(loss)) {
					printf("Simulated loss %d\n", in.seq);
					curr = 0;
					continue;
				} else if (simFault(corrupt)) {
					printf("Simulated corruption %d\n", in.seq);
					curr = 0;
					continue;
				}

				printPkt(in, 0);

				// slide window forward if possible
				if (in.type != 2)
					printf("IGNORE %d: not ACK packet\n", in.seq);
				else if (in.seq < seq_base || in.seq > seq_base + win_size)
					printf("IGNORE %d: unexpected ACK\n", in.seq);
				else if (in.seq >= seq_base) {
					seq_base = in.seq + 1;
					printf("Sliding window to %d/%d\n", seq_base, total);
					curr = 0;
				}
			} else {
				if (curr >= win_size || seq_base + curr >= total) {
					printf("Timeout on %d\n", seq_base);
					curr = 0;
				} else if (seq_base + curr >= total)
					continue;

				bzero((char *) &out, sizeof(out));
				out.type = 1; // data packet
				out.seq = seq_base + curr;
				fseek(file, out.seq * PACKET_SIZE, SEEK_SET);
				out.size = fread(out.data, 1, PACKET_SIZE, file);

				// send next packet in window
				if (sendto(sockfd, &out, sizeof(out), 0,
						(struct sockaddr*) &cli_addr, clilen) < 0)
					error("ERROR sending packet\n");
				printPkt(out, 1);
				curr++;
			}
		}

		printf("All packets sent and acknowledged\n");

		// send FIN
		bzero((char *) &out, sizeof(out));
		out.type = 3; // FIN packet
		out.seq = seq_base;
		out.size = 0;
		if (sendto(sockfd, &out, sizeof(out), 0,
				(struct sockaddr*) &cli_addr, clilen) < 0)
			error("ERROR sending FIN\n");
		printPkt(out, 1);

		// wait for FIN ACK, resend if necessary
		while (1) {
			FD_ZERO(&readset);
			FD_SET(sockfd, &readset);
			if (select(sockfd+1, &readset, NULL, NULL, &timeout) < 0) {
				error("ERROR on select\n");
			} else if (FD_ISSET(sockfd, &readset)) {
				if (recvfrom(sockfd, &in, sizeof(in), 0,
						(struct sockaddr*) &cli_addr, &clilen) < 0) {
					printf("Packet lost\n");
					continue;
				}
				printPkt(in, 0);

				if (in.type == 3 && in.seq == out.seq)
					break;
			} else {
				if (sendto(sockfd, &out, sizeof(out), 0,
						(struct sockaddr*) &cli_addr, clilen) < 0)
					error("ERROR sending FIN\n");
				printPkt(out, 1);
			}
		}

		// cleanup
		printf("File transfer complete\n");
		printf("----------------------------------------\n");
		fclose(file);

		// ready for new file request
	}

	// unreachable
	close(sockfd);
	return 0;
}
