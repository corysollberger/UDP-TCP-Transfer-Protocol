udp: udpServer.c udpClient.c
	gcc udp -o udpServer.o udpClient.o -l pthread