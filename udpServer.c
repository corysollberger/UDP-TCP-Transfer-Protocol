#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <semaphore.h>

int freeThrow, err = 0; //The UDP Socket

int currentACK = 0; //Cumulative Acknowledgements
int resend[5000]; //A buffer that holds the packet # to resend
int done = 0;
int32_t count = 0;
FILE* fp; //file descriptor
sem_t ACKS;

//Thread IDS
pthread_t tid[2];

//client
int tcpClientPortNum = 2011;
char* tcpClientIp = "127.0.0.1";

//server
int tcpServerPortNum = 2011;
char* tcpServerIp = "127.0.0.1";

//UDP
int udpServerPortNum = 2012;
char* udpServerIp = "127.0.0.1";

//Handles the TCP packets being sent to acknowledge 
void* serverTCP(){
	//Allocate two sockets
	int socket1 = socket(AF_INET, SOCK_STREAM, 0);
	int socket2 = socket(AF_INET, SOCK_STREAM, 0);

	//The sockaddr that will hold the internet address
	struct sockaddr_in servera, clienta;
	servera.sin_family = AF_INET;
	servera.sin_port = htons(tcpServerPortNum);
	inet_pton(AF_INET, tcpServerIp, &(servera.sin_addr));

	//bind a socket, and test if theres an error
	err = bind(socket1, (struct sockaddr *) &servera, sizeof(servera));

	listen(socket1, 1); //Put the socket into listening mode

	if (err != 0){
		exit(0);
	}

	//Create the buffer that will hold the message
	char buffer[4];
	int len = sizeof(servera);
	socket2 = accept(socket1, (struct sockaddr *) &servera, &len);

	printf("Watching for dropped packets or successful completion...\n");
	while(buffer[2] != 'L'){
		int s = recv(socket2, buffer, 4, 0);
		if (s > 0){ //Ack Received
			printf("Acknowledgement: %d||||||||||||||||||||||\n", ntohl(*(uint32_t*)buffer));
			sem_wait(&ACKS);
			count = ntohl(*(uint32_t*)buffer);
			fseek(fp, 4092*(count), SEEK_SET);
			sem_post(&ACKS);
		}
	}
	//send(socket2,"ALL", 4, 0);
	printf("DONE\n");
	done = 1;
	close(socket2);
	close(socket1);
}

void* serverUDP(){
	
	printf("pThread: UDP Initialized\n");

	int err;
	
	//Allocate one socket
	freeThrow = socket(AF_INET, SOCK_DGRAM, 0);

	//Sockaddr(s) hold the internet address of the contact
	struct sockaddr_in me, clientad; //
	me.sin_family = AF_INET;
	me.sin_port = htons(2012);
	inet_pton(AF_INET, "127.0.0.1", &(me.sin_addr));

	int slen = sizeof(clientad);

	//bind a socket, and test if theres an error
	err = (bind(freeThrow, (struct sockaddr *) &me, sizeof(me)) == 1);

	if (err != 0){
		printf("GoodBye");
		exit(0);
	}

	char buff[100];
	
	//Wait for Message from a client to perform some action
	while(1){
		printf("Waiting for communication...\n");
		err = recvfrom(freeThrow, buff, sizeof(buff), 0, (struct sockaddr *) &clientad, &slen);

		if (err == -1){
			printf("error: recvfrom\n");
		}
		sem_wait(&ACKS);
		char *f_name = buff; //Specify the name of the file to be sent

		fp = fopen(f_name,"rb"); //open file

		//Find File Size (Bytes)
		fseek(fp, 0L, SEEK_END);
		int filesize = ftell(fp);
		rewind(fp);
		int fs = 0;
		int numPackets = 0;
		int packetSize = 4096;
		int dataSize = packetSize - 4; //Calculates the Difference once the header is applied

		//Calculate # of Messages
		if (filesize%dataSize == 0){
			numPackets = (filesize/dataSize);
		} else {
			numPackets = ((filesize/dataSize)+1);
		}

		printf("Filesize: %d, numpackets: %d\n", filesize, numPackets);
		int fsbytes = (filesize + (4*numPackets));
		printf("# of messages: %d total filesize: %d\n", numPackets, fsbytes);

		//Send FileSize to Client
		int intBuff;
		intBuff = fsbytes;
		if (err = sendto(freeThrow, &intBuff, sizeof(int), 0, (struct sockaddr*) &clientad, slen)<0){
				printf("ERROR: FileSize failed to send...");
		}
		intBuff = numPackets;

		//Send # of Packets to Client
		if (err = sendto(freeThrow, &intBuff, sizeof(int), 0, (struct sockaddr*) &clientad, slen)<0){
				printf("ERROR: # of Packets failed to send...");
		}
		

		if(fp == NULL) //Error Check
            	{
                	printf("ERROR: File %s not found.\n", f_name);
                	exit(1);
            	}

		//The buffer that will hold the bytes to be sent
		unsigned char buffMe[4096];

		int val;
		int nbyt;
		
		int curNum = 0;
		sem_post(&ACKS);
		while (done == 0){
			sem_wait(&ACKS);
			memcpy(&buffMe, &count, sizeof(int32_t));
			int nums = 0;
			val = fread((buffMe+4), sizeof(char), (4096-4), fp);
			if (nbyt = sendto(freeThrow, buffMe, val+4, 0, (struct sockaddr*) &clientad, slen)<0){
				printf("ERROR: File, %s, failed to send...", f_name);
			} else {
			}
			if (val == 0){
			}
			sem_post(&ACKS);
			bzero(buffMe, 4096);
			count++;
		}
		printf("File should've sent!\n");
		fclose(fp);

	}
}

main(){
	//Initialize the Resending Array with 0's
	int i = 0;
	for(i=0;i<5000;i++){
		resend[i] = 0;
	}

	//Initialize Semaphore
	sem_init(&ACKS, 1, 1);

	//Thread Creation
	pthread_create(&tid[0], NULL, serverTCP, NULL);
	pthread_create(&tid[1], NULL, serverUDP, NULL);
	(void) pthread_join(tid[1], NULL);
	//serverUDP();
}