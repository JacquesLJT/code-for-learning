#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define PORT 8080
#define CONTENT_TYPE_SIZE 64
#define HEADER_SIZE 1024

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
                    "Last-Modified: %s\r\n\r\n",
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
    int bytes_sent, bytes_received;
    char buffer[1024] = {0};
    char *response;
    char *header;
    char *content_type;

    // Create a new socket
    printf("\nCreating socket...\n");

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    printf("\nSetting socket options...\n");

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set address information
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the address
    printf("\nBinding socket to address...\n");

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    printf("\nListening on port %d, press Ctrl+C to stop\n\n", PORT);

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connections and send the requested webpage
    while(1) {

        printf("\033[1;32mWaiting for incoming connections...\033[0m\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Connection accepted\n\n");

        valread = recv(new_socket, buffer, 1024, 0);

        if (valread < 0) {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }

        printf("Message received:\n %s\n", buffer);

        // Check if the request is a file upload
        if (strstr(buffer, "POST /upload") != NULL) {

            printf("\033[1;32mHandling file upload...\033[0m\n");

            // Extract the uploaded file data from the message
            char *content_type_start = strstr(buffer, "Content-Type: ");
            char *content_type_end = strstr(content_type_start, "\r\n");
            char content_type[64];
            strncpy(content_type, content_type_start + 14, content_type_end - content_type_start - 14);
            content_type[content_type_end - content_type_start - 14] = '\0';

            char *filename_start = strstr(buffer, "filename=\"");
            char *filename_end = strstr(filename_start + 10, "\"");
            char filename[256];
            strncpy(filename, filename_start + 10, filename_end - filename_start - 10);
            filename[filename_end - filename_start - 10] = '\0';

            char *file_data_start = strstr(buffer, "\r\n\r\n") + 4;
            int file_data_size = valread - (file_data_start - buffer);

            // Save the uploaded file to the server
            char filepath[256];
            sprintf(filepath, "uploads/%s", filename);

            FILE *fp = fopen(filepath, "wb");
            fwrite(file_data_start, 1, file_data_size, fp);
            fclose(fp);

            // Send the HTTP response
            char *response_body = malloc(256);
            sprintf(response_body, "File uploaded: %s", filepath);

            char *header = malloc(1024);
            sprintf(header, "HTTP/1.1 201 Created\r\n"
                            "Content-Length: %ld\r\n"
                            "Content-Type: text/plain\r\n\r\n",
                    strlen(response_body));

            bytes_sent = send(new_socket, header, strlen(header), 0);
            if (bytes_sent < 0) {
                fprintf(stderr, "\033[1;31mError sending header: %s\033[0m\n", strerror(errno));
                exit(EXIT_FAILURE);
            } else {
                printf("\033[1;32mHeader Sent!\033[0m\n");
            }

            bytes_sent = send(new_socket, response_body, strlen(response_body), 0);
            if (bytes_sent < 0) {
                fprintf(stderr, "\033[1;31mError sending response body: %s\033[0m\n", strerror(errno));
                exit(EXIT_FAILURE);
            } else {
                printf("\033[1;32mResponse body sent!\033[0m\n");
            }

            free(response_body);
            free(header);

        } else {

            // Find the requested webpage in the incoming header
            char *start = strstr(buffer, "GET /");
            char *end = strstr(buffer, " HTTP/");
            int len = end - (start + 5);
            char filename[len+1];
            strncpy(filename, start+5, len);
            filename[len] = '\0';

            // Open the requested file and read its contents
            FILE *fp = fopen(filename, "rb");

            // If the file exists, read it and send it to the client
            if (fp) {

                printf("Requested file: %s\n", filename);

                // Get the content type of the requested file
                content_type = malloc(CONTENT_TYPE_SIZE);
                memset(content_type, 0, CONTENT_TYPE_SIZE);

                get_content_type(content_type, filename);

                // Create the header for the response
                header = malloc(HEADER_SIZE);
                memset(header, 0, HEADER_SIZE);

                create_header(header, content_type, filename);

                // Read the file contents
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                response = malloc(size + 1);
                fread(response, 1, size, fp);
                response[size] = '\0';
                fclose(fp);

            }
                // If no file is requested, return the default page
            else if (strcmp(filename, "") == 0) {

                printf("\033[1;33mNo file requested, returning default page: index.html\033[0m\n\n");

                // If no file is requested, return the default page
                content_type = "text/html";

                header = malloc(HEADER_SIZE);
                memset(header, 0, HEADER_SIZE);

                create_header(header, content_type, "index.html");

                // Read the file contents
                fp = fopen("index.html", "rb");
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                response = malloc(size + 1);
                fread(response, 1, size, fp);
                response[size] = '\0';
                fclose(fp);
            }
                // If the file cannot be opened, return a 404 response
            else {

                // If the file cannot be opened, return a 404 response

                printf("\033[1;31mFile not found: %s\033[0m\n", filename);

                content_type = "text/html";
                header = malloc(HEADER_SIZE);
                memset(header, 0, HEADER_SIZE);

                create_header(header, content_type, "404.html");

                // Read the file contents
                fp = fopen("404.html", "rb");
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                response = malloc(size + 1);
                fread(response, 1, size, fp);
                response[size] = '\0';
                fclose(fp);
            }

            int response_length = strlen(response);

            // Send the header to the client
            bytes_sent = send(new_socket, header, strlen(header), 0);
            if (bytes_sent < 0) {
                fprintf(stderr, "\033[1;31mError sending header: %s\033[0m\n", strerror(errno));
                exit(EXIT_FAILURE);
            } else {
                printf("\033[1;32mHeader Sent!\033[0m\n");
            }

            // Send the response to the client
            bytes_sent = send(new_socket, response, response_length, 0);
            if (bytes_sent < 0) {
                fprintf(stderr, "\033[1;31mError sending response: %s\033[0m\n", strerror(errno));
                exit(EXIT_FAILURE);
            } else {
                printf("\033[1;32mResponse Sent!\033[0m\n");
            }

            fwrite(response, 1, response_length, stdout);
            fwrite("\n", 1, 1, stdout);

            shutdown(new_socket, SHUT_RDWR);
            printf("\033[1;33mConnection closed, waiting for new connection...\033[0m\n\n");

            free(header);
            free(response);

        }






    }

    // Close the socket
    shutdown(server_fd, SHUT_RDWR);

    return 0;
}
