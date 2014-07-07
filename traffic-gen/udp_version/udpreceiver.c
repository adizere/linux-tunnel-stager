/*
 * .author: forest@cs.ubbcluj.ro, 2004
 *
 * .problem: this program receives data from a server
 * it connects to with a special bitrate it specifies
 *
 * .licence: GNU GPL
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define time 120
#define SIZE_OF_CHUNK 1024

int bytes_received = 0;
int sock;

int check_bytes_received(int sig) {
    printf("%d bytes received after %d secs.\n", bytes_received, time);	
    bytes_received = 0;

    //close(sock);
    //exit(0);
    signal(SIGALRM, check_bytes_received);
    alarm(1);
}

main(int argc, char* argv[]) {
    int bandwidth, i;
    struct sockaddr_in srv_address;
    char *chunk;
    
    chunk = (char *) malloc(SIZE_OF_CHUNK);
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset((char *) &srv_address, 0, sizeof(struct sockaddr_in));
    srv_address.sin_family = AF_INET;
    srv_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost is not good!
    srv_address.sin_port = htons(2001);

    if (connect(sock, (struct sockaddr *) &srv_address, sizeof(struct sockaddr_in)) < 0) {
	perror("error connecting to the server: ");
    }	
    printf("everything is ok so far..\n");
    printf("%d\n", argc);
    bandwidth = atoi(argv[1]);
    i = sendto(sock, &bandwidth, sizeof(int), 0, (struct sockaddr *) &srv_address, sizeof(struct sockaddr_in));
    printf("am trimis la server %d bytes.\n",i);
    
    signal(SIGALRM, check_bytes_received);
    //alarm(time);
    alarm(1);
    while(1) {
	i = recvfrom(sock, chunk, SIZE_OF_CHUNK, 0, 0, 0);
	if (i==-1) perror("error at receive: ");
	bytes_received += (i>0? i: 0);
	//printf("bytes_received = %d\n",bytes_received);
    }    
    
    //close(sock); 
    	    
}

