all: client1 client2 server 
remake: clean client1 client2 server 
client1:
	gcc -g client1.c constants.h -o client1
client2:
	gcc -g client2.c constants.h -o client2
server:
	gcc -g server.c constants.h -lpthread -o server
clean:
	rm -f client1 client2 server
	
