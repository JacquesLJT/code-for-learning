#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8080

// Create the http response header
void create_header(char *header, const char *content_type, const char *file_path) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) < 0) {
        perror("stat failed");
        exit(EXIT_FAILURE);
    }

    time_t t = time(NULL);
    struct tm *timeinfo = gmtime(&t);

    // Format the date and time for the header
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S %Z", timeinfo);

    // Set the http response header
    sprintf(header, "HTTP/1.1 200 OK\r\n"
                    "Date: %s\r\n"
                    "Server: webserver\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "Last-Modified: %s\r\n"
                    "Connection: close\r\n\r\n",
                    date_str, content_type, file_stat.st_size, ctime(&file_stat.st_mtime));
}

// Find the content type of the requested file
void get_content_type(char *content_type, const char *file_path) {
    if (strstr(file_path, ".jpg") != NULL) {
        strcpy(content_type, "image/jpeg");
    }
    else if (strstr(file_path, ".png") != NULL) {
        strcpy(content_type, "image/png");
    }
    else if (strstr(file_path, ".txt") != NULL) {
        strcpy(content_type, "text/plain");
    }
    else if (strstr(file_path, ".css") != NULL) {
        strcpy(content_type, "text/css");
    }
    else if (strstr(file_path, ".js") != NULL) {
        strcpy(content_type, "application/javascript");
    }
    else {
        strcpy(content_type, "text/html");
    }
}

int main(int argc, char **argv) {

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char *html = "<html><body><h1>Hello, world!</h1></body></html>";
    char *response;
    char *header;
    char *content_type;

    // Create a new socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set address information
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connections and send the requested webpage
    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        valread = recv(new_socket, buffer, 1024, 0);

        if (valread < 0) {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }

        printf("Message received: %s\n", buffer);

        // Find the requested webpage in the incoming header
        char *start = strstr(buffer, "GET /");
        char *end = strstr(buffer, " HTTP/");
        int len = end - (start + 5);
        char filename[len+1];
        strncpy(filename, start+5, len);
        filename[len] = '\0';

        // Open the requested file and read its contents
        FILE *fp = fopen(filename, "rb");

        if (fp) {

            printf("Requested file: %s\n", filename);

            // Get the content type of the requested file
            content_type = malloc(64);
            get_content_type(content_type, filename);

            // Create the header for the response
            header = malloc(1024);
            create_header(header, content_type, filename);

            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            response = malloc(size + 1);
            fread(response, 1, size, fp);
            response[size] = '\0';
            fclose(fp);

        } else if (strcmp(filename, "") == 0) {

            printf("No file requested, returning default page: index.html\n");

            // If no file is requested, return the default page
            content_type = "text/html";
            header = malloc(1024);
            create_header(header, content_type, "index.html");

            fp = fopen("index.html", "rb");
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            response = malloc(size + 1);
            fread(response, 1, size, fp);
            response[size] = '\0';
            fclose(fp);
        } else {
            // If the file cannot be opened, return a 404 response
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        }

        // Send the response to the client
        int response_length = strlen(response);

        send(new_socket, header, strlen(header), 0);
        send(new_socket, response, response_length, 0);

        fwrite(response, 1, response_length, stdout);
        fwrite("\n", 1, 1, stdout);
        fwrite(html, 1, strlen(html), stdout);
        fclose(stdout);
        shutdown(new_socket, SHUT_RDWR);
        free(response);
    }

    return 0;
}
