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

//TODO
//WHEN RECEIVING A PACKET, SEND TCP ACK
//IF NO ACK IS RECEIVED, RESEND PACKET
//SEND ALLDONE when all Packets are Received
//ALLDONE -> Close Program(Close fp, loop back to accepting state)

pthread_t tid[2]; //Holds the ID of the threads

int currentACK = 0; //Cumulative Acknowledgements
int resend[5000]; //A buffer that holds the packet # to resend
int ALLDONE = 0;
int flagMiss = 0; //0 = no miss, 1 = miss
sem_t ACKS;
//TCP Values

//client
int tcpClientPortNum = 2011;
char* tcpClientIp = "127.0.0.1";

//server
int tcpServerPortNum = 2011;
char* tcpServerIp = "127.0.0.1";

//UDP
int udpServerPortNum = 2012;
char* udpServerIp = "127.0.0.1";

void* tcpClient(){
	//Only uses one socket
	int socket1 = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sa;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(tcpServerPortNum);
	inet_pton(AF_INET, tcpServerIp, &(sa.sin_addr));
	
	int len = sizeof(sa);
	int x = connect(socket1, (struct sockaddr *) &sa, sizeof(sa));
	
	if (x!=0){
		exit(0);
	}

	//Now that we have a connected socket, we can send messages to the server
	//char *msg = "Hello from the otherside!";
	int tcpSuccess = 0;
	int curACK = currentACK;
	char buffer[4];
	while(tcpSuccess == 0){
		if(ALLDONE == 1){
			send(socket1,"ALL", 4, 0);
			int s = recv(socket1, buffer, 4, 0);
			if(s>0){
				printf("Size of Bytes Receieved: %d, Message: %s\n", s, buffer);
				tcpSuccess = 1;
			}
		} else if (flagMiss == 1) { //When we acknowledge missed packets let server know
			uint32_t un = htonl(currentACK);
			send(socket1, &un, sizeof(uint32_t), 0);
		}
		sleep(01);
	}

	close(socket1);
}

void* udpClient(){
	printf("UDP Initialized\n");
	struct sockaddr_in sa_server;
	int err;
	char buffer[100];
	sa_server.sin_family = AF_INET;
	sa_server.sin_port = htons(udpServerPortNum);
	err = inet_pton(AF_INET, udpServerIp, &(sa_server.sin_addr));
	if (err != 1){
		exit(1);
	}

	int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	
	//Get User Input to select file to ask server for
	char input[100];

	printf("Please enter the name of the file to send with extension\n");
	scanf("%s",input);

	int slen = sizeof(sa_server);

	int b = sendto(serverSocket, input, sizeof(input), 0, (struct sockaddr*) &sa_server, slen);
	printf("MSG Size: %d Bytes\n", b);
	printf("UDP Message sent to server: %s\n", input);

	int buffSize = 50;
	char returnBuffer[buffSize];

	int recvlen;

	FILE *fp = fopen("RESULTS.txt", "w");
	
	printf("||||||Beginning File Transfer||||||\n");

	

	int intBuff; int fileSize = 0; int chunks = 0;

	//Receive FileSize from Server
	int c = recvfrom(serverSocket, &intBuff, sizeof(intBuff), 0, (struct sockaddr *) &sa_server, &slen);
	fileSize = intBuff;
	printf("FileSize: %d\n", fileSize);
	//bzero(intBuff, 4);

	//Receive # of packets from Server
	c = recvfrom(serverSocket, &intBuff, sizeof(intBuff), 0, (struct sockaddr *) &sa_server, &slen);
	chunks = intBuff;
	printf("Packets: %d\n", chunks);
	int packets[chunks];

	int i = 0;
	//Initialize values to 0
	for (i=0;i<chunks;i++){
		packets[i] = 0;
	}
	
	//Condition      //The # of bytes expected   //the current packet
	int done = 0; int sizeofdata = fileSize; int curNum = 0;

	//Handles the transfer of data
	while(done == 0){
		recvlen = recvfrom(serverSocket, returnBuffer, sizeof(returnBuffer), 0, (struct sockaddr *) &sa_server, &slen);
		sem_wait(&ACKS);
		if(*(int*)returnBuffer != currentACK){
			if(flagMiss == 0){
				printf("currentACK: %d", currentACK);
			}
			flagMiss = 1; //Miss
			sem_post(&ACKS);
		} else {
			flagMiss = 0; //reset flag
			currentACK = currentACK + 1;
			printf("currentACK: %d", currentACK);
			packets[*(int*)returnBuffer] = 1;
			if (recvlen == -1){ //error handling
				printf("Recvlen error: BEEP BOOP\n");
				exit(0);
			} else if (recvlen == 0){ //No data retrieved from buffer
				printf("||||||File Transfer Complete...||||||\n");
				done = 1;
			} else { //Iterate
				printf("Bytes Received: %d\n", recvlen);
				sizeofdata = sizeofdata - (recvlen);
				int write = fwrite(returnBuffer+4, sizeof(char), (recvlen-4), fp);
				bzero(returnBuffer, buffSize);
				if (sizeofdata == 0){ //Bytes left == 0
					printf("||||||File Transfer Complete...||||||\n");
					done = 1;
				}
			}
			sem_post(&ACKS);
		}
	}
	i = 0;
	int complete = 1; //1 = True 0 = False, if false resubmit dropped packets
	printf("Packets: 0 = Dropped, 1 = Accepted\n");
	for (i=0;i<chunks;i++){
		//printf("Packet %d: %d\n", i, packets[i]);
		printf("%d", packets[i]);
		if(packets[i] != 1){
			complete = 0;
		}
		if (i%71 == 0 && i != 0){
			printf("\n");
		}
	}
	if (complete == 1){
		printf("\n");
		printf("The Data Transfer was Successful!\n");
		ALLDONE = 1;
		
	} else {
		printf("We are missing some packets!\n");
	}
	fclose(fp);	
}

main(int argc, char* argv[]){
	int i = 0;

	//server
	char* tcpServerIp = argv[1];
	int tcpServerPortNum = (int)strtol(argv[2], NULL, 0);
	

	//client
	char* tcpClientIp = argv[3];
	int tcpClientPortNum = (int)strtol(argv[4], NULL, 0);
	

	//UDP
	char* udpServerIp = argv[5];
	int udpServerPortNum = (int)strtol(argv[6], NULL, 0);

	for(i=0;i<5000;i++){
		resend[i] = 0;
	}

	printf("-TCPServerIP: %s\n", tcpServerIp);
	printf("-TCPServerPort: %d\n\n", tcpServerPortNum);
	printf("+TCPClientIP: %s\n", tcpClientIp);
	printf("+TCPClientPort: %d\n\n", tcpClientPortNum);
	printf("_UDPServerIp: %s\n", udpServerIp);
	printf("_UDPServerPort: %d\n\n", udpServerPortNum);

	//Initialize Semaphore
	sem_init(&ACKS, 1, 1);

	pthread_create(&tid[0], NULL, tcpClient, NULL);
	pthread_create(&tid[1], NULL, udpClient, NULL);
	(void) pthread_join(tid[1], NULL);
	//udpClient();
}