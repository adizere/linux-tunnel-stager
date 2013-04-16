/*
 * .author: forest@cs.ubbcluj.ro, 2004
 *
 * .purpose: this program sends data to every client that
 * connects to it data, useing a certain bandwidth specified
 * by the client.
 *
 * .licence: GNU GPL
 *
 */


/*
 * The length of the TCP header is minimum 20 bytes.
 * The length of the IP header is minimum 20 bytes.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#define SIZE_OF_CHUNK 1024
#define MIN_SLEEP_TIME 0.001		// in s.

main(int argc, char **argv) {
   int sock, bandwidth, len_addr, i, j, k, data, packet_size;
   struct sockaddr_in address;
   struct timespec sleepts, tmp;
   struct timeval now;
   double begin_time, end_time, send_time, next_send_time, pkt_gap, t; // all in seconds
   char *chunk;
   int sec;

   // we send 256 KB chunks
   chunk = (char *) malloc(SIZE_OF_CHUNK);
   memset((char *) chunk, 0, SIZE_OF_CHUNK);
   sock = socket(AF_INET, SOCK_STREAM, 0);

    memset((char *) &address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(argv[1]); // localhost is not good!
    address.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *) &address, sizeof(struct sockaddr_in)) < 0) {
      perror("error connecting to the server: ");
    }
    printf("everything is ok so far..\n");
    printf("%d\n", argc);
    bandwidth = 100000000;
    i = send(sock, &bandwidth, sizeof(int), 0);
    printf("am trimis la server %d bytes.\n",i);

   printf("sending data with a bitrate of %d bytes/sec...\n",bandwidth);
   data = 1;
   packet_size = SIZE_OF_CHUNK;
   pkt_gap= ((double) packet_size)/bandwidth;
   // for start I'm sending only sizeof(int)+20+20 bytes at a time
   //packet_size = sizeof(int) + 20 + 20;
   printf("setting SIZE_OG_CHUNCK=%d bytest PACKET_GAP=%.4f sec.\n", packet_size, pkt_gap);
   sec = 0;
   next_send_time = 0;
   while(1) {
	gettimeofday(&now, 0);
	begin_time = now.tv_sec + now.tv_usec/1000000.0;
	if (next_send_time == 0) next_send_time = begin_time;
	j = 0;
	for(i=0; i<bandwidth/packet_size; i++) {
		gettimeofday(&now, 0);
	        send_time = now.tv_sec + now.tv_usec/1000000.0;
		j += ((k=send(sock, chunk, SIZE_OF_CHUNK, 0))>0? k: 0);
		if (k==-1) perror("error at send: ");

		next_send_time = next_send_time + pkt_gap;
		t = next_send_time - send_time;
		if (t >= MIN_SLEEP_TIME) {
			sleepts.tv_sec = (unsigned int) t;
			sleepts.tv_nsec = (t - sleepts.tv_sec) * 1000000000;
			nanosleep(&sleepts, &tmp);
		}
	}
	j += ((k=send(sock, chunk, bandwidth%packet_size, 0))>0? k: 0);
	if (k==-1) perror("error at send: ");
	gettimeofday(&now, 0);
	end_time = now.tv_sec + now.tv_usec/1000000.0;
	printf("sent %d bytes (%d send calls) in second %d, at time %.4f.\n", j, i+(k>0? 1:0),
		sec, end_time - begin_time);
	sec++;
   }
}

