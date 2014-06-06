/*
	tincanGBN
	===================
	Packet data structure.
*/

#define PACKET_SIZE 1024

struct packet {
  int type;	// 0: Request, 1: Data, 2: ACK, 3: FIN
  int seq;	// Packet sequence number
  int size;	// Data size
  char data[PACKET_SIZE];
};
