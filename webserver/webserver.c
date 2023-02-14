#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

#define BUFFER_SIZE	512

/* Default port to listen on */
#define DEFAULT_PORT	31337

int main(int argc, char **argv) {

	int socket_fd,new_socket_fd;
	struct sockaddr_in server_addr, client_addr;
	int port=8080;
	int n,on=1;
	socklen_t client_len;
	char buffer[BUFFER_SIZE];
	char header[BUFFER_SIZE];
	char file[BUFFER_SIZE];
	char temp[100];
	char* time_str;
	char* last_modified_time;
	char* content_type;
	int content_length, header_size = 0;

	FILE *fp;
	int fh;

	/* get current time for header */
	time_t t = time(NULL);
	time_str = ctime(&t);

	struct stat file_stat;

	printf("Starting server on port %d\n",port);

	/* Open a socket to listen on */
	/* AF_INET means an IPv4 connection */
	/* SOCK_STREAM means reliable two-way connection (TCP) */
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd<0) {
		fprintf(stderr,"Error opening socket! %s\n",
			strerror(errno));
		exit(1);
	}

	/* avoid TIME_WAIT */
	setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on));

	/* Set up the server address to listen on */
	/* The memset stes the address to 0.0.0.0 which means */
	/* listen on any interface. */
	memset(&server_addr,0,sizeof(struct sockaddr_in));
	server_addr.sin_family=AF_INET;
	/* Convert the port we want to network byte order */
	server_addr.sin_port=htons(port);

	/* Bind to the port */
	if (bind(socket_fd, (struct sockaddr *) &server_addr,
		sizeof(server_addr)) <0) {
		fprintf(stderr,"Error binding! %s\n", strerror(errno));
		fprintf(stderr,"Probably in time wait, have to wait 60s if you ^C to close\n");
		exit(1);
	}

	/* Tell the server we want to listen on the port */
	/* Second argument is backlog, how many pending connections can */
	/* build up */
	listen(socket_fd,5);

wait_for_connection:

	/* Call accept to create a new file descriptor for an incoming */
	/* connection.  It takes the oldest one off the queue */
	/* We're blocking so it waits here until a connection happens */
	client_len=sizeof(client_addr);
	new_socket_fd = accept(socket_fd,
			(struct sockaddr *)&client_addr,&client_len);
	if (new_socket_fd<0) {
		fprintf(stderr,"Error accepting! %s\n",strerror(errno));
		exit(1);
	}

	while(1) {

		/* Someone connected!  Let's try to read BUFFER_SIZE-1 bytes */
		memset(buffer,0,BUFFER_SIZE);

		n = read(new_socket_fd,buffer,(BUFFER_SIZE-1));

		/* If n is 0 the connection to client is lost*/
		if (n==0) {
			fprintf(stderr,"Connection to client lost\n\n");
			break;
		}
		/* If n < 0 then there is an error with the socket */
		else if (n<0) {
			fprintf(stderr,"Error reading from socket %s\n",
				strerror(errno));
		}
		/* If n == BUFFER_SIZE-1 then we might have truncated the message */
		else if (n == (BUFFER_SIZE-1)) {
			fprintf(stderr,"Buffer too small, message truncated\n");
		}

		/* Need to get the name of the file after the GET request */
		char *file_name = strstr(buffer, "GET /");
		file_name = file_name + 5;
		char *end = strstr(file_name, " ");
		*end = '\0';
		printf("file_name: %s\n", file_name);

		/* If the file name is empty, set it to index.html */
		if (strcmp(file_name, "") == 0) {
			file_name = "index.html";
		}

		/* Open the file */
		fh = open(file_name, O_RDONLY);
		if (fh == -1) {
			
			/* If the file doesn't exist, send a 404 error */
			strcpy(header, "HTTP/1.1 404 Not Found\r\n");
			strcat(header, "Date: ");
			strcat(header, time_str);
			strcat(header, "Server: ECE435\r\n");
			strcat(header, "Content-Length: 150\r\n");
			strcat(header, "Connection: close\r\n");
			strcat(header, "Content-Type; charset=iso-8859-1\r\n");
			strcat(header, "\r\n");

			/* error html */
			char* error = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";

			/* find the size of the header */
			header_size = strlen(header);

			/* write the header to the socket and check for errors */
			n = write(new_socket_fd,header,header_size);
			if (n<0) {
				fprintf(stderr,"Error writing to socket %s\n",
					strerror(errno));
			}

			/* write the error to the socket and check for errors */
			n = write(new_socket_fd,error,strlen(error));
			if (n<0) {
				fprintf(stderr,"Error writing to socket %s\n",
					strerror(errno));
			}

			break;
		}

		/* Read the requested file and check for errors */
		fp = fdopen(fh, "r");
		if (fp == NULL) {
			printf("Error opening file\n");
			break;
		}

		/* set the file buffer to 0 */
		memset(file,0,BUFFER_SIZE);

		/* read the file into the buffer */
		fread(file, 1, BUFFER_SIZE, fp);
		if (ferror(fp)) {
			printf("Error reading file\n");
			break;
		}

		/* close the file */
		fclose(fp);

		/* populate the stat struct on the requested file*/
		stat(file_name, &file_stat);

		/* get the last modified time and content length*/
		last_modified_time = ctime(&file_stat.st_mtime);
		content_length = file_stat.st_size;
		
		/* determine the currect content type */
		if (strstr(file_name, ".jpg") != NULL) {
			content_type = "image/jpeg";
		}
		else if (strstr(file_name, ".png") != NULL) {
			content_type = "image/png";
		}
		else if (strstr(file_name, ".txt") != NULL) {
			content_type = "text/plain";
		} 
		else {
			content_type = "text/html";
		}

		/* create the header */
		strcpy(header, "HTTP/1.1 200 OK\r\n");
		strcat(header, "Date: ");
		strcat(header, time_str);
		strcat(header, "Server: ECE435\r\n");
		strcat(header, "Last-Modified: ");
		strcat(header, last_modified_time);
		sprintf(temp, "Content-Length: %d\r\n", content_length);
		strcat(header, temp);
		strcat(header, "Content-Type: ");
		strcat(header, content_type);
		strcat(header, "\r\n\r\n");

		/* find the size of the header */
		header_size = strlen(header);

		/* write the header to the socket and check for errors */
		n = write(new_socket_fd,header,header_size);
		if (n<0) {
			fprintf(stderr,"Error writing. %s\n",
				strerror(errno));
		}

		/* write the file to the socket and check for errors */
		n = write(new_socket_fd,file,content_length);
		if (n<0) {
			fprintf(stderr,"Error writing. %s\n",
				strerror(errno));
		}

		break;

	}

	close(new_socket_fd);

	printf("Done connection, go back and wait for another\n\n");

	goto wait_for_connection;

	/* Close the sockets */
	close(socket_fd);

	return 0;
}
