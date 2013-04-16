/*
 * .author: forest@cs.ubbcluj.ro, 2004
 *
 * .problem: this program receives data from a server
 * it connects to with a special bitrate it specifies
 *
 * .licence: GNU GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>

#define time 120
#define SIZE_OF_CHUNK 1024

int bytes_received = 0;
int sock;

void check_bytes_received(int sig) {
    printf("%d bytes received after %d secs.\n", bytes_received, time);
    bytes_received = 0;

    //close(sock);
    //exit(0);
    signal(SIGALRM, check_bytes_received);
    alarm(1);
}

main(int argc, char* argv[]) {
    int bandwidth, i, data, sd;
    struct sockaddr_in address, client;
    char *chunk;

    chunk = (char *) malloc(SIZE_OF_CHUNK);
    sock = socket(AF_INET, SOCK_STREAM, 0);

   memset((char *) &address,0,sizeof(struct sockaddr_in));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = htonl(INADDR_ANY);
   address.sin_port = htons(atoi(argv[1]));

   if (bind(sock, (struct sockaddr *) &address, sizeof(struct sockaddr_in)) < 0) {
    perror("error at bind: ");
   }
   listen(sock, 5);
   printf("receiver is up and running ..\n");

   int len_addr = sizeof(struct sockaddr);
   sd = accept(sock, (struct sockaddr *) &client, &len_addr);
   if (sd<0) {
    perror("error accepting connection: ");
   }
   printf("accepting client.\n");
   i = recv(sd, &bandwidth, sizeof(int), 0);
   if (i<0) {
    perror("error reading bandwidth: ");
   }

    signal(SIGALRM, check_bytes_received);
    //alarm(time);
    alarm(1);
    while(1) {
    	i = recv(sd, chunk, SIZE_OF_CHUNK, 0);
    	if (i==-1) perror("error at receive: ");
    	bytes_received += (i>0? i: 0);
    	//printf("bytes_received = %d\n",bytes_received);
    }

    //close(sock);

}

