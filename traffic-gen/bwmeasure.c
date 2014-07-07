#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>		// for IP header
#include <netinet/if_ether.h>	// for Ethernet header
#include <netinet/tcp.h>	// for TCP header
#include <netinet/udp.h>	// for UDP header


struct {
	unsigned int udp_bw, tcp_bw, udp_pkts, tcp_pkts;
} stats __attribute__ ((aligned));
double start_dtime = 0;
FILE *fout;
pcap_t *capt;


void print_bw(int sig) {
	struct timeval tv_now;
	double now_dtime;

        gettimeofday(&tv_now, NULL);
        now_dtime = (double) tv_now.tv_sec + tv_now.tv_usec/1000000.0;
	printf("%.4f ms:	TCP_BW: %u (%u pkts), UDP_BW: %u (%u pkts)\n", 
		now_dtime - start_dtime, stats.tcp_bw, stats.tcp_pkts, stats.udp_bw, stats.udp_pkts);	

	fprintf(fout, "%.4f ms:	TCP_BW: %u (%u pkts), UDP_BW: %u (%u pkts)\n", 
		now_dtime - start_dtime, stats.tcp_bw, stats.tcp_pkts, stats.udp_bw, stats.udp_pkts);
	fflush(fout);	
	bzero(&stats, sizeof(stats));
	fflush(stdout);
	signal(SIGALRM, print_bw);
	//alarm(1);
}

void start(int sig) {
	struct timeval tv_now;
	if (start_dtime == 0) {
		printf("starting report intervals ..\n");
		gettimeofday(&tv_now, NULL);
                start_dtime = (double) tv_now.tv_sec + tv_now.tv_usec/1000000.0;
		alarm(1);
	}
}

void close(int sig) {
	struct pcap_stat ps;
	fprintf(stderr, "SIGINT caught!\n");
	if (fout) fclose(fout);
	pcap_stats(capt, &ps);
	fprintf(stderr, "%d packets received\n", ps.ps_recv);
	fprintf(stderr, "%d packets dropped\n", ps.ps_drop);

	exit(1);		
}

void handle_pckt(u_char *userdata, const struct pcap_pkthdr *pkthdr,
		const u_char *pkt) {
	register struct iphdr *ip_hdr;
	register struct tcphdr *tcp_hdr;
	register struct udphdr *udp_hdr;
	char proto[10], flags[4];
	unsigned short sport, dport, off;
	u_char *buff;
	struct in_addr addr_1, addr_2;
//	double seconds = pkthdr->ts.tv_sec + (double) pkthdr->ts.tv_usec/1000.0;

	// printf receipt time, length of the captured data, total length of packet 
//	printf("packet received: %.4f %d %d\n", seconds, pkthdr->caplen, pkthdr->len);
	
	buff = pkt + sizeof(struct ether_header);	// jump over Ethernet header
	ip_hdr = (struct iphdr*) buff;
	addr_1.s_addr=ip_hdr->saddr;
	addr_2.s_addr=ip_hdr->daddr;

	// the following bit operations are done directly in network byte order
	if (ip_hdr->frag_off & 0x0040) {
		strcpy(flags, "DF");			// Don't Fragment flag
		if (ip_hdr->frag_off & 0x00a0)
			strcat(flags, "!");		// either Reserved bit or More Fragments bit set!
	} else {
		if (ip_hdr->frag_off & 0x0020)
			strcpy(flags, "MF");		// More Fragments flag
		else
			strcpy(flags, "LF");		// Last Fragment
	}
	off = htons(ip_hdr->frag_off & 0xff1f); 
	
	if (ip_hdr->protocol == IPPROTO_TCP) {
		tcp_hdr = (struct tcphdr*) (buff + sizeof(struct iphdr));
		sport = ntohs(tcp_hdr->source);
		dport = ntohs(tcp_hdr->dest);
        	//payload_length = ntohs(ip_hdr->tot_len) - ip_hdr->ihl*4 - tcp_hdr->doff*4;
		strcpy(proto, "TCP");
		stats.tcp_bw += ntohs(ip_hdr->tot_len);
		stats.tcp_pkts++;
	}
	if (ip_hdr->protocol == IPPROTO_UDP) {
		udp_hdr = (struct udphdr*) (buff + sizeof(struct iphdr));
		sport = ntohs(udp_hdr->source);
		dport = ntohs(udp_hdr->dest);
		//payload_length = ntohs(udp_hdr->len) - 8;
		strcpy(proto, "UDP");
		stats.udp_bw += ntohs(ip_hdr->tot_len);
		stats.udp_pkts++;
	}

//	fprintf(fout, "%d bytes: %s", ntohs(ip_hdr->tot_len), inet_ntoa(addr_1));
//	if (strcmp(flags, "LF") != 0) fprintf(fout, ":%d",sport);
//	fprintf(fout, " -> %s", inet_ntoa(addr_2));
//	if (strcmp(flags, "LF") != 0) fprintf(fout, ":%d", dport);
//	fprintf(fout," [IP: ID=%d, FLAGS=%s, OFF=%d]        (%s)", ntohs(ip_hdr->id), 
//		flags, off, proto);
//	if (ip_hdr->protocol == IPPROTO_TCP) {
//		fprintf(fout, " SEQ[%u], ACK_SEQ[%u]\n", ntohl(tcp_hdr->seq), ntohl(tcp_hdr->ack_seq));
//	} else {
//		fprintf(fout, "\n");
//	}
}

main(int argc, char* argv[]) {
	char errbuf[PCAP_ERRBUF_SIZE];
	int err;
	char *user;
	struct bpf_program fp;
	bpf_u_int32 netmask = 0xffffff00;
	struct timeval tv_now;
	struct itimerval timer;

	signal(SIGALRM, print_bw);
	signal(SIGUSR1, start);
	signal(SIGINT, close);
	//initialize statistics
	bzero(&stats, sizeof(stats));

	//open dump file
	fout = fopen("bw.log", "w+");
	
	capt = pcap_open_live("eth0", 68, 0, 10, errbuf);
	if (!capt) {
		perror("error opening iface: ");
	} 
//	pcap_compile(capt, &fp, "src 192.168.1.2 and \(tcp dst port 2000 or udp\)",
//			0, netmask);
//	pcap_compile(capt, &fp, "src 143.205.140.54 and udp", 0, netmask);
//	pcap_compile(capt, &fp, "src 143.205.140.54 and \(tcp dst port 22 or udp\)",
//			0, netmask);
	//printf("errmsg = %s\n", pcap_geterr(capt));
//	pcap_setfilter(capt, &fp);
	//printf("errmsg = %s\n", pcap_geterr(capt));

	/* bandwidth reports are displayed on the screen one time per second.
	 * the reports can be started in two ways:
	 *	o either by specifying the "start" parameter in the comand line
	 *	o or when a SIGUSR1 signal is caught.
	 */
	if ((argc == 2) && (strcmp(argv[1], "start") == 0)) {
		sleep(2);
	        gettimeofday(&tv_now, NULL);
	        start_dtime = (double) tv_now.tv_sec + tv_now.tv_usec/1000000.0;
		//alarm(1);
		timer.it_interval.tv_sec = 1;
		timer.it_interval.tv_usec = 0;
		timer.it_value = timer.it_interval;
		setitimer(ITIMER_REAL, &timer, 0);
		fprintf(stderr, "starting report intervals ..\n");	
	}
	if (capt) {	
		err = pcap_loop(capt, -1, handle_pckt, user); 
		printf("err = %d\n", err);
	} 

	pcap_close(capt);
}

