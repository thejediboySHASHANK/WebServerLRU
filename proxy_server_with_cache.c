#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_CLIENTS 10
#define MAX_BYTES 4096
#define MAX_ELEMENT_SIZE 10*(1<<10)
#define MAX_SIZE 200*(1<<20)
typedef struct cache_element cache_element;

// Defining Cache elements to be stored in the LRU (Least Recently Used) cache
struct cache_element {
    char* data; // response data is stored in this field
    int len; // data bytes information
    char* url; // url of the request
    time_t lru_time_track; // time of the last access
    cache_element* next; // pointer to the next cache element
};

cache_element* find(char* url);
int add_cache_element(char* data, int size, char* url);
void remove_cache_element();

int port_number = 8080;
int proxy_socketId;
pthread_t tid[MAX_CLIENTS];
sem_t semaphore; // semaphore to control the number of clients
pthread_mutex_t lock; // mutex lock to prevent race conditions in multi-threaded environment

// For defining the head of the Cache LL
cache_element* head;
int cache_size;

int connectRemoteServer(char* host_addr, int port_num) {
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocket < 0) {
        printf("Error in creating socket\n");
        return -1;
    }
    struct hostent* host = gethostbyname(host_addr);
    if (host==NULL) {
        fprintf(stderr, "Error, no such host\n");
        return -1;
    }
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num); // converts the port number to network byte order

    bcopy((char*)&host->h_addr, (char*)&server_addr.sin_addr.s_addr, host->h_length); // copies the host address to the server address
    if (connect(remoteSocket, (struct sockaddr*)&server_addr, (size_t)sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error in connecting to remote server\n");
        return -1;
    }

    return remoteSocket;
}

int handle_request(int clientSocketId, ParsedRequest* request, char* tempReq) {
    char* buf = (char*) malloc(MAX_BYTES * sizeof(char));
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    if (ParsedHeader_set(request, "Connection", "close") < 0) {
        printf("Set Connection Header is not working\n");
    }

    size_t len = strlen(buf);
    if (ParsedHeader_get(request, "Host") == NULL) {
        if (ParsedHeader_set(request, "Host", request->host) < 0) {
            printf("Set Host Header is not working\n");
        }
    }

    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0) {
        printf("Unparse Headers is not working\n");
    }

    int server_port = 80;
    if (request->port != NULL) {
        server_port = atoi(request->port);
    }
    int remoteSocketId = connectRemoteServer(request->host, server_port);
    if (remoteSocketId < 0) {
        return -1;
    }

    int bytes_send_server = send(remoteSocketId, buf, strlen(buf), 0);
    bzero(buf, MAX_BYTES);

    bytes_send = recv(remoteSocketId, buf, MAX_BYTES-1, 0);
    char *temp_buffer = (char*) malloc(MAX_BYTES * sizeof(char));
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while(bytes_send > 0) {
        bytes_send = send(clientSocketId, buf, bytes_send, 0);
        for (int i = 0; i < bytes_send/sizeof(char); i++) {
            temp_buffer[temp_buffer_index] = buf[i];
            temp_buffer_index++;
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char*) realloc(temp_buffer, temp_buffer_size);
        if (bytes_send < 0) {
            perror("Error in sending the response to the client");
            break;
        }
        bzero(buf, MAX_BYTES);
        bytes_send = recv(remoteSocketId, buf, MAX_BYTES-1, 0);
    }
    temp_buffer[temp_buffer_index] = '\0';
    free(buf);
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    free(temp_buffer);
    close(remoteSocketId);
    return 0;
}

int checkHTTPversion(char* msg) {
    int version = -1;

    if (strncmp(msg, "HTTP/1.0", 8) == 0) {
        version = 1;
    } else if (strncmp(msg, "HTTP/1.1", 8) == 0) {
        version = 1;
    } else {
        version = -1;
    }

    return version;
}

int sendErrorMessage(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  printf("400 Bad Request\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  printf("403 Forbidden\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  printf("404 Not Found\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //printf("500 Internal Server Error\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  printf("501 Not Implemented\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  printf("505 HTTP Version Not Supported\n");
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

	}
	return 1;
}

void* thread_fn(void* socketNew) {
    sem_wait(&semaphore); // decrement the semaphore value
    int p;
    sem_getvalue(&semaphore, p);
    printf("semaphore value is: %d\n", p);
    int *t = (int*) socketNew;
    int socket = *t;
    int bytes_send_client, len;

    char* buffer = (char*) calloc(MAX_BYTES, sizeof(char)); // buffer to store the request
    bzero(buffer, MAX_BYTES); // initialising the buffer with 0
    bytes_send_client = recv(socket, buffer, MAX_BYTES, 0); // receiving the request from the client

    while (bytes_send_client > 0) {
        len = strlen(buffer);
        if (strstr(buffer, "\r\n\r\n") == NULL) {
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        } else {
            break;
        }
    }

    char* tempReq = (char*) malloc(strlen(buffer) * sizeof(char));
    for (int i = 0; i < strlen(buffer); i++) {
        tempReq[i] = buffer[i];
    }
    struct cache_element* temp = find(tempReq); // checking if the request is already in the cache
    if (temp != NULL) {
        int size = temp->len/sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];
        while(pos->size) {
            bzero(response, MAX_BYTES);
            for (int i = 0; i < MAX_BYTES; i++) {
                response[i] = temp->data[i];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("Data retrieved from cache\n");
        printf("%s\n\n", response);
    } else if (bytes_send_client > 0) {
        len = strlen(buffer);
        struct ParsedRequest* request = ParsedRequest_create(); // creating a new parsed request object

        if (ParsedRequest_parse(request, buffer, len) < 0) {
            // If there is an error in parsing the request, the program prints an error message and exits with a status of 1.
            perror("Error in parsing the request");
            exit(1);
        } else {
            bzero(buffer, MAX_BYTES);
            if (!strcmp(request->method, "GET")){
                if (request->port &&  request->path && checkHTTPversion(request->version)==1) {
                    bytes_send_client - handle_request(socket, request, tempReq);
                    if (bytes_send_client < 0) {
                        // If there is an error in sending the response to the client, the program sends 500 status
                        sendErrorMessage(socket, 500);
                    }
                } else {
                    sendErrorMessage(socket, 500);
                }
            } else {
                printf("This code doesn't support any other methods apart from GET\n");
            }
        }
        ParsedRequest_destroy(request); // destroying the parsed request object
    } else if (bytes_send_client == 0) {
        printf("Client disconnected\n");
    }
    shutdown(socket, SHUT_RDWR); // shutting down the socket
    close(socket); // closing the socket
    free(buffer); // freeing the buffer
    sem_post(&semaphore); // increment the semaphore value
    sem_getvalue(&semaphore, p);
    printf("Semaphore post value is: %d\n", p);
    free(tempReq);
    return NULL;
}

int main(int argc, char* argv[]) {
    int client_socketId, client_length;
    struct sockaddr_in server_addr, client_addr;
    sem_init(&semaphore, 0, MAX_CLIENTS);

    // Initialising the lock
    pthread_mutex_init(&lock, NULL);

    if (argv == 2) {
        port_number = atoi(argv[1]);
    } else {
        printf("Too few arguments\n");
        exit(1); // system call to terminate the process
    }

    printf("Proxy server started on port %d\n", port_number);

    // Creating a new socket and storing the socket descriptor in `proxy_socketId`.
    // The `socket` function takes three arguments:
    // 1. `AF_INET`: This is the address family that is used to designate the type of addresses that your socket can communicate with (in this case, Internet Protocol v4 addresses).
    // 2. `SOCK_STREAM`: This is the type of socket. `SOCK_STREAM` means it is a TCP socket, which is a type of socket that provides a reliable, stream-oriented connection.
    // 3. `0`: This argument is the protocol that is to be used with the socket. A value of `0` means that the operating system will choose the most appropriate protocol based on the socket type. In this case, it will choose TCP for a `SOCK_STREAM` type socket.
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);

    // Checking if the socket was successfully created. If the socket creation failed, `socket` function will return -1.
    // If the socket creation failed, the program prints an error message and exits with a status of 1.
    if (proxy_socketId < 0) {
        perror("Error in creating socket");
        exit(1);
    }

    // Setting the `SO_REUSEADDR` socket option. This allows the socket to reuse the address and port for subsequent connections.
    // This is useful in a scenario where a server has been shut down and then restarted right away while sockets are still active on its port.
    // Without this option, the server would fail to bind to the port because it will be locked by the Operating System.
    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        // If there is an error in setting the socket option, the program prints an error message and exits with a status of 1.
        perror("Error in setting socket options");
        exit(1);
    }

    bzero((char*)&server_addr, sizeof(server_addr)); // initialising the server address with 0
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number); // converting the port number to network byte order
    server_addr.sin_addr.s_addr = INADDR_ANY; // setting the IP address of the server to INADDR_ANY, which allows the server to accept connections from any client.
    if (bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        // If there is an error in binding the socket to the address and port, the program prints an error message and exits with a status of 1.
        perror("Port is not available");
        exit(1);
    }
    printf("Binding on port %d\n", port_number);
    int listen_status = listen(proxy_socketId, MAX_CLIENTS); // listening for incoming connections
    if (listen_status < 0) {
        // If there is an error in listening for incoming connections, the program prints an error message and exits with a status of 1.
        perror("Error in listening");
        exit(1);
    }

    int i = 0;
    int Connected_socketId[MAX_CLIENTS];

    while (1) {
        bzero((char*)&client_addr, sizeof(client_addr));
        client_length = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_length);
        if (client_socketId < 0) {
            // If there is an error in accepting the connection, the program prints an error message and exits with a status of 1.
            perror("Error in accepting connection");
            exit(1);
        } else {
            Connected_socketId[i] = client_socketId; // add the client socket descriptor to the array
        }

        struct sockaddr_in* client_ptr = (struct sockaddr_in*)&client_addr; // typecasting the client address to sockaddr_in
        struct in_addr ip_addr = client_ptr->sin_addr; // getting the IP address of the client
        char str[INET_ADDRSTRLEN]; // defining a string to store the IP address
        inet_ntop(AF_INET, &ip_addr, str, INET6_ADDRSTRLEN); // converting the IP address to a human-readable format
        printf("Client is connected with port number %d and IP address %s\n", ntohs(client_ptr->sin_port), str);

        pthread_create(&tid[i], NULL, thread_fn, (void*)& Connected_socketId[i]); // creating a new thread to handle the client
        i++;
    }
    close(proxy_socketId); // closing the proxy server socket
    return 0;
}

cache_element* find(char* url) {
    cache_element* site = NULL;
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove cache lock acquired %d\n", temp_lock_cal);
    if (head != NULL) {
        site = head;
        while(site != NULL) {
            if (strcmp(site->url, url) == 0) {
                printf("LRU time track before: %ld\n", site->lru_time_track);
                printf("\n url found\n");
                site->lru_time_track = time(NULL);
                printf("LRU time track after: %ld\n", site->lru_time_track);
                break;
            }
            site = site->next;
        }
    } else {
        printf("Url not found, Cache is empty\n");
    }
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove cache lock released %d\n", temp_lock_val);

    return site;
}

void remove_cache_element() {
    cache_element *p;
    cache_element *q;
    cache_element *temp;

    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove cache lock acquired %d\n", temp_lock_val);
    if (head != NULL) {
        for(p = head, q = head, temp = head; q->next != NULL; q = q->next) {
            if (((q->next)->lru_time_track) < (temp->lru_time_track)) {
                temp = q->next;
                p = q;
            }
        }
        if (temp == head) {
            head = head->next;
        } else {
            p->next = temp->next;
        }
        cache_size = cache_size - (temp->len) - sizeof(cache_element) - strlen(temp->url) - 1;
        free(temp->data);
        free(temp->url);
        free(temp);
    }

    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove cache lock released %d\n", temp_lock_val);
    return;
}

int add_cache_element(char *data, int size, char *url) {
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Add cache lock acquired %d\n", temp_lock_val);
    int element_size = size+1+strlen(url)+sizeof(cache_element);

    if (element_size > MAX_ELEMENT_SIZE) {
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add cache lock released %d\n", temp_lock_val);
        return 0;
    }
    else {
        while(cache_size + element_size > MAX_SIZE) {
            remove_cache_element();
        }
        cache_element* element = (cache_element*) malloc(sizeof(cache_element));
        element -> data = (char*) malloc(size+1);
        strcpy(element->data, data);
        element->url = (char*) malloc(1 + (strlen(url)+sizeof(char)));
        strcpy(element->url, url);
        element->lru_time_track = time(NULL);
        element->next = head;
        element->len = size;
        head = element;
        cache_size += element_size;
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add cache lock released %d\n", temp_lock_val);
        return 1;
    }
    return 0;
}


