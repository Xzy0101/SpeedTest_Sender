/*
48. Write a test program that uses the socket interface to send messages between
a pair of Unix workstations connected by some LAN (e.g., Ethernet, 802.11). Use
this test program to perform the following experiments:
(a) Measure the round-trip latency of TCP and UDP for different message sizes
(e.g., 1 byte, 100 bytes, 200 bytes , . . . , 1000 bytes).
(b) Measure the throughput of TCP and UDP for 1-KB, 2-KB, 3-KB, . . . , 32-KB
messages. Plot the measured throughput as a function of message size.
(c) Measure the throughput of TCP by sending 1 MB of data from one host to another.
Do this in a loop that sends a message of some size¡ªfor example, 1024 iterations
of a loop that sends 1-KB messages. Repeat the experiment with different
message sizes and plot the results.
*/

/*
This is the sender part.
*/

#include<stdlib.h>
#include<stdio.h>
#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<chrono>

#pragma comment(lib,"ws2_32.lib")

#define PORT 5432
#define PORT2 5433
#define K 1024
#define M 1024*1024
#define ACK "a"

#define MAX_HOSTNAME 32
#define N 100

char data[M];	//too large to be in a function

long long total_time(SOCKET s, char* msg, int msg_len, int flag, int n)
//return the total time of sending msg and receiving ACK for n times, in microseconds
{
	char buf[2];
	auto t0 = std::chrono::system_clock::now();
	for (int i = 0; i < n; ++i) {
		send(s, msg, msg_len, flag);
		recv(s, buf, 2, 0);
		if (buf[0] != ACK[0]) {
			printf("Receive error: not ACK");
			--i;	//resend the message
		}
	}
	auto t = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(t - t0).count();
}

int main()
{
	/*initialize data*/
	srand((unsigned int)time(NULL));
	for (int i = 0; i < M; ++i) {
		data[i] = rand();
	}

	/*select protocol type*/
	int af, type, protocol;
	char mode;
	printf("Protocol('t' for TCP, 'u' for UDP): ");
	std::cin >> mode;
	switch (mode)
	{
	case 't':
		af = AF_INET;
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
		break;
	case 'u':
		af = AF_INET;
		type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
		break;
	default:
		printf("Protocol not supported: %c\n", mode);
		exit(1);
	}

	/*initialize WSA*/
	WSADATA wsaData;
	WORD version = MAKEWORD(2, 2);	//WSA version 2.2
	int iresult;	//WSA function result
	if ((iresult = WSAStartup(version, &wsaData)) != 0) {
		printf("WSA initialize: Error %d\n", iresult);
		exit(1);
	}

	/*get receiver address*/
	char hostname[MAX_HOSTNAME];
	printf("Hostname: ");
	std::cin >> hostname;
	addrinfo hint;
	addrinfo* result = NULL;
	ZeroMemory(&hint, sizeof(hint));
	hint.ai_family = af;
	hint.ai_socktype = type;
	hint.ai_protocol = protocol; 
	if ((iresult = getaddrinfo(hostname, NULL, &hint, &result)) != 0) {	
		printf("Getaddrinfo: Error %d, unknown host: %s\n", iresult, hostname);
		exit(1);
	}

	/*build connection socket*/
	sockaddr_in sin;
	memcpy((char *)&sin, (char*)(result->ai_addr), sizeof(sin));
	sin.sin_family = af;
	sin.sin_port = htons(PORT);
	freeaddrinfo(result);

	SOCKET s;
	if ((s = socket(af, type, protocol)) == SOCKET_ERROR) {
		printf("Socket: Error %d\n", WSAGetLastError());
		return 1;
	}

	/*connect to receiver*/
	if (connect(s, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR) {
		printf("Connect: Error %d\n", WSAGetLastError());
		closesocket(s);
		return 1;
	}

	/*do the experiment*/
	/*(a) Measure the round-trip latency of TCP and UDP for different message sizes
	(e.g., 1 byte, 100 bytes, 200 bytes, . . ., 1000 bytes).*/
	FILE* rslt;
	fopen_s(&rslt, "result.txt", "w");
	fprintf(rslt, "receiver: ");
	fprintf(rslt, hostname);
	fprintf(rslt, "\n%d \t byte RTT: %lld \tusec\n", 1, total_time(s, data, 1, 0, N)/N);
	fprintf(rslt, "%d \t byte RTT: %lld \tusec\n", 100, total_time(s, data, 100, 0, N)/N);
	fprintf(rslt, "%d \t byte RTT: %lld \tusec\n", 200, total_time(s, data, 200, 0, N)/N);
	fprintf(rslt, "%d \t byte RTT: %lld \tusec\n", 1000, total_time(s, data, 1000, 0, N)/N);
	fprintf(rslt, "\n");
	/*(b) Measure the throughput of TCP and UDP for 1-KB, 2-KB, 3-KB, . . . , 32-KB
	messages. Plot the measured throughput as a function of message size.*/
	double throughput;
	for (int size = 1; size < 33; ++size) {
		throughput = (size*K * 8 * N) / (total_time(s, data, size*K, 0, N) / 1000000.0) / 1000;
		fprintf(rslt, "%d \t KB throughput: %f\tkbps\n", size, throughput);
	}
	fprintf(rslt, "\n");
	/*(c) Measure the throughput of TCP by sending 1 MB of data from one host to another.
	Do this in a loop that sends a message of some size¡ªfor example, 1024 iterations
	of a loop that sends 1-KB messages. Repeat the experiment with different
	message sizes and plot the results.*/
	if (protocol == IPPROTO_TCP) {
		throughput = (M * 8) / (total_time(s, data, M, 0, 1) / 1000000.0) / 1000;
		fprintf(rslt, "1 MB throughput: %f\tkbps\n", throughput);
		long long sum = 0;
		for (int i = 0; i < K; ++i) {
			sum += total_time(s, &data[i*K], K, 0, 1);
		}
		throughput = (M * 8) / (sum / 1000000.0) / 1000;
		fprintf(rslt, "1024 1 KB throughput: %f\tkbps\n", throughput);
	}

	/*clean up*/
	fclose(rslt);
	closesocket(s);
	WSACleanup();
	return 0;
}