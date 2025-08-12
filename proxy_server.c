#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>


#define MAX_CLIENTS 350  //max number of client requests served at a time
#define MAX_BYTES 4096   //max allowed size of request/response
#define MAX_SIZE 200*(1<<20)     //size of the cache
#define MAX_ELEMENT_SIZE 10*(1<<20)     //max size of an element in cache

pthread_t t_id[MAX_CLIENTS];
int port_number = 8080;
int proxy_socket_ID;
sem_t semaphore;
pthread_mutex_t lock; // lock is used for locking cache
 
typedef struct cache_store cache_store;

struct cache_store{
    char* data;
    int len;
    char* url;
    time_t lru_time_record;
    cache_store* next;
};


cache_store* find(char* url);
int add_cache_store(char* data, int size, char* url);
void remove_cache_store();
cache_store* head;
int cache_size;


cache_store* find(char* url){

    // Checks for URL in the cache; if found, returns pointer to the respective cache element, otherwise returns NULL
    cache_store* site = NULL;
    
    // Acquire lock 🛠️
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("🔒 Cache Lock Acquired (Status: %d)\n", temp_lock_val);
    
    if(head != NULL) {
        site = head;
        while (site != NULL) {
            if(!strcmp(site->url, url)) {
                printf("🎯 URL found! 🔗 [%s]\n", url);
                
                // Logging LRU Time before update ⏲️
                printf("⏱️ LRU Time Before Update: %ld\n", site->lru_time_record);

                // Updating the LRU time track
                site->lru_time_record = time(NULL);
                
                // Logging LRU Time after update ⏲️
                printf("✅ LRU Time After Update: %ld\n", site->lru_time_record);
                break;
            }
            site = site->next;
        }
    } else {
        printf("❌ URL not found in cache: [%s]\n", url);
    }

    // Release lock 🛠️
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("🔓 Cache Lock Released (Status: %d)\n", temp_lock_val);
    
    return site;
}





void remove_cache_store(){
    // If cache is not empty, searches for the node which has the least lru_time_record and deletes it
    cache_store *p;   // Previous cache_store pointer
    cache_store *q;   // Next cache_store pointer
    cache_store *temp; // Cache element to remove
    
    // Acquire lock 🛠️
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("🔒 Cache Lock Acquired for Removal (Status: %d)\n", temp_lock_val);

    if(head != NULL) { // Cache is not empty
        printf("🔍 Searching for the oldest cached element...\n");

        // Start traversal to find the least recently used cache element
        for(q = head, p = head, temp = head; q->next != NULL; q = q->next) {
            // Compare LRU times and track the oldest
            if((q->next->lru_time_record) < (temp->lru_time_record)) {
                temp = q->next;
                p = q;
            }
        }

        if(temp == head) {
            printf("🗑️ Oldest element is at the head. Removing it...\n");
            head = head->next; // Handle base case where head is the oldest
        } else {
            printf("🗑️ Oldest element found in cache. Removing: [%s]\n", temp->url);
            p->next = temp->next;
        }

        // Update the cache size after removal
        cache_size = cache_size - (temp->len) - sizeof(cache_store) - strlen(temp->url) - 1;
        printf("📏 Updated Cache Size: %d bytes\n", cache_size);

        // Free the memory allocated for the removed cache element
        free(temp->data);
        free(temp->url);
        free(temp);

        printf("✅ Cache element removed successfully!\n");
    } else {
        printf("⚠️ Cache is already empty, nothing to remove.\n");
    }

    // Release lock 🛠️
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("🔓 Cache Lock Released after Removal (Status: %d)\n", temp_lock_val);
}






int add_cache_store(char* data, int size, char* url) {
    // Adds an element to the cache
    
    // Acquire lock 🛠️
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("🔒 Add Cache Lock Acquired (Status: %d)\n", temp_lock_val);

    // Calculate the total size of the new cache element
    int element_size = size + 1 + strlen(url) + sizeof(cache_store);
    
    if (element_size > MAX_ELEMENT_SIZE) {
        // Element size exceeds the maximum allowed cache size
        printf("❌ Element too large to cache! Size: %d bytes (Max allowed: %d bytes)\n", element_size, MAX_ELEMENT_SIZE);

        // Release lock 🛠️
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("🔓 Add Cache Lock Released (Status: %d)\n", temp_lock_val);

        return 0;
    } else {
        // Remove elements until there's enough space to add the new one
        while (cache_size + element_size > MAX_SIZE) {
            printf("⚠️ Cache full! Removing least recently used element to make space...\n");
            remove_cache_store();
        }

        // Allocate memory for the new cache element
        cache_store* element = (cache_store*) malloc(sizeof(cache_store));
        element->data = (char*)malloc(size + 1); // Memory for the cached data
        strcpy(element->data, data);
        
        // Allocate memory for the URL
        element->url = (char*)malloc(1 + (strlen(url) * sizeof(char)));
        strcpy(element->url, url);
        
        // Update LRU time
        element->lru_time_record = time(NULL);
        printf("🕒 Cache Element Time Track Set: %ld\n", element->lru_time_record);

        // Insert the new element at the head of the cache
        element->next = head;
        element->len = size;
        head = element;

        // Update cache size
        cache_size += element_size;
        printf("📦 New element added to cache! Total cache size: %d bytes\n", cache_size);

        // Release lock 🛠️
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("🔓 Add Cache Lock Released (Status: %d)\n", temp_lock_val);

        return 1;
    }
    
    return 0;
}




int sendErrorMessage(int socket, int status_code) {
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    // Get the current time in the proper format
    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code) {
        case 400: 
            // 400 Bad Request
            snprintf(str, sizeof(str), 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Length: 150\r\n"
                "Connection: keep-alive\r\n"
                "Content-Type: text/html\r\n"
                "Date: %s\r\n"
                "Server: CustomServer/14785\r\n\r\n"
                "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n"
                "<BODY><H1>🚫 400 - Bad Request!</H1>\n"
                "<P>It seems your request couldn't be processed due to an invalid format. "
                "Please check your request syntax and try again.</P></BODY></HTML>", 
                currentTime);
            printf("⚠️ Oops! 400 Bad Request (Client Error)\n");
            send(socket, str, strlen(str), 0);
            break;

        case 403: 
            // 403 Forbidden
            snprintf(str, sizeof(str), 
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Length: 180\r\n"
                "Connection: keep-alive\r\n"
                "Content-Type: text/html\r\n"
                "Date: %s\r\n"
                "Server: CustomServer/14785\r\n\r\n"
                "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n"
                "<BODY><H1>🚫 403 - Access Denied!</H1>\n"
                "<P>You don't have permission to access the requested resource. "
                "If you believe this is an error, contact the site administrator.</P></BODY></HTML>", 
                currentTime);
            printf("🚫 403 Forbidden (Access Denied!)\n");
            send(socket, str, strlen(str), 0);
            break;

        case 404: 
            // 404 Not Found
            snprintf(str, sizeof(str), 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 160\r\n"
                "Connection: keep-alive\r\n"
                "Content-Type: text/html\r\n"
                "Date: %s\r\n"
                "Server: CustomServer/14785\r\n\r\n"
                "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
                "<BODY><H1>🔍 404 - Page Not Found!</H1>\n"
                "<P>Sorry, the page you're looking for doesn't exist. "
                "Please check the URL or return to the homepage.</P></BODY></HTML>", 
                currentTime);
            printf("🔍 404 Not Found (Page Missing!)\n");
            send(socket, str, strlen(str), 0);
            break;

        case 500: 
            // 500 Internal Server Error
            snprintf(str, sizeof(str), 
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Length: 200\r\n"
                "Connection: keep-alive\r\n"
                "Content-Type: text/html\r\n"
                "Date: %s\r\n"
                "Server: CustomServer/14785\r\n\r\n"
                "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n"
                "<BODY><H1>💥 500 - Server Error!</H1>\n"
                "<P>Something went wrong on our end. We're working on fixing the issue. "
                "Please try again later or contact support.</P></BODY></HTML>", 
                currentTime);
            printf("💥 500 Internal Server Error (Something went wrong on our end!)\n");
            send(socket, str, strlen(str), 0);
            break;

        case 501: 
            // 501 Not Implemented
            snprintf(str, sizeof(str), 
                "HTTP/1.1 501 Not Implemented\r\n"
                "Content-Length: 170\r\n"
                "Connection: keep-alive\r\n"
                "Content-Type: text/html\r\n"
                "Date: %s\r\n"
                "Server: CustomServer/14785\r\n\r\n"
                "<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n"
                "<BODY><H1>🚧 501 - Feature Not Implemented!</H1>\n"
                "<P>Sorry, the feature you're trying to use is not implemented yet. "
                "Stay tuned for future updates!</P></BODY></HTML>", 
                currentTime);
            printf("❗ 501 Not Implemented (Feature coming soon!)\n");
            send(socket, str, strlen(str), 0);
            break;

        case 505: 
            // 505 HTTP Version Not Supported
            snprintf(str, sizeof(str), 
                "HTTP/1.1 505 HTTP Version Not Supported\r\n"
                "Content-Length: 190\r\n"
                "Connection: keep-alive\r\n"
                "Content-Type: text/html\r\n"
                "Date: %s\r\n"
                "Server: CustomServer/14785\r\n\r\n"
                "<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n"
                "<BODY><H1>🔄 505 - Unsupported HTTP Version!</H1>\n"
                "<P>Your browser is using an HTTP version that this server doesn't support. "
                "Please upgrade your browser or try a different one.</P></BODY></HTML>", 
                currentTime);
            printf("🔄 505 HTTP Version Not Supported (Please upgrade your browser!)\n");
            send(socket, str, strlen(str), 0);
            break;

        default:
            printf("❓ Unknown status code: %d\n", status_code);
            return -1;
    }
    return 1;
}



int connectRemoteServer(char* host_addr, int port_number) {
    // Create socket for remote server
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocket < 0) {
        perror("🚨 Error: Failed to create socket");
        return -1;
    }
    printf("✅ Socket created successfully. (Socket FD: %d)\n", remoteSocket);

    // Get host by the provided hostname or IP address
    struct hostent *host = gethostbyname(host_addr);
    if (host == NULL) {
        fprintf(stderr, "❌ Error: No such host \"%s\" exists.\n", host_addr);
        close(remoteSocket);
        return -1;
    }
    printf("🔍 Host found: %s\n", host_addr);

    // Insert IP address and port number of the host into `server_addr`
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    bcopy((char *)host->h_addr, (char *)&server_addr.sin_addr.s_addr, host->h_length);

    printf("🌐 Preparing to connect to %s on port %d...\n", inet_ntoa(server_addr.sin_addr), port_number);

    // Connect to remote server
    if (connect(remoteSocket, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0) {
        perror("❌ Error: Failed to connect to remote server");
        close(remoteSocket);
        return -1;
    }
    
    printf("🔗 Successfully connected to remote server at %s on port %d.\n", inet_ntoa(server_addr.sin_addr), port_number);
    
    // Return the socket descriptor for further communication
    return remoteSocket;
}


int handle_request(int clientSocket, ParsedRequest *request, char *tempReq) {
    char *buf = (char*)malloc(sizeof(char) * MAX_BYTES);
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    // Setting the "Connection" header to close
    if (ParsedHeader_set(request, "Connection", "close") < 0) {
        printf("⚠️ Warning: Failed to set 'Connection' header to 'close'.\n");
    }

    // Ensure the "Host" header is set
    if (ParsedHeader_get(request, "Host") == NULL) {
        if (ParsedHeader_set(request, "Host", request->host) < 0) {
            printf("⚠️ Warning: Failed to set 'Host' header.\n");
        }
    }

    // Unparse the headers
    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0) {
        printf("⚠️ Warning: Failed to unparse headers.\n");
        // Still try to send the request without the header
    }

    int server_port = 80; // Default remote server port
    if (request->port != NULL) {
        server_port = atoi(request->port);
    }

    // Connect to the remote server
    int remoteSocketID = connectRemoteServer(request->host, server_port);
    if (remoteSocketID < 0) {
        printf("❌ Error: Unable to connect to remote server at %s:%d.\n", request->host, server_port);
        return -1;
    }
    printf("🔗 Connected to remote server %s on port %d.\n", request->host, server_port);

    // Send the request to the remote server
    int bytes_send = send(remoteSocketID, buf, strlen(buf), 0);
    if (bytes_send < 0) {
        perror("❌ Error: Failed to send request to remote server.\n");
        close(remoteSocketID);
        free(buf);
        return -1;
    }
    printf("📤 Sent request to server (%d bytes).\n", bytes_send);

    // Prepare to receive the response
    bzero(buf, MAX_BYTES);
    bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    char *temp_buffer = (char*)malloc(sizeof(char) * MAX_BYTES); // Temp buffer
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while (bytes_send > 0) {
        // Send the response to the client
        bytes_send = send(clientSocket, buf, bytes_send, 0);
        if (bytes_send < 0) {
            perror("❌ Error: Failed to send data to client socket.\n");
            break;
        }
        printf("📨 Sent data to client socket (%d bytes).\n", bytes_send);

        // Store the response in the temp buffer
        for (int i = 0; i < (int)(bytes_send / sizeof(char)); i++) {
            temp_buffer[temp_buffer_index] = buf[i];
            temp_buffer_index++;
        }

        // Reallocate temp buffer if needed
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char*)realloc(temp_buffer, temp_buffer_size);

        // Zero out the buffer and prepare for the next chunk of data
        bzero(buf, MAX_BYTES);
        bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    }

    // Null-terminate the temp buffer and store in cache
    temp_buffer[temp_buffer_index] = '\0';
    add_cache_store(temp_buffer, strlen(temp_buffer), tempReq);
    printf("🗃️ Response added to cache (size: %lu bytes).\n", strlen(temp_buffer));

    // Clean up
    free(buf);
    free(temp_buffer);
    close(remoteSocketID);
    printf("✅ Request handling completed.\n");

    return 0;
}



int checkHTTPversion(char *msg){
    int version = -1;

    if(strncmp(msg, "HTTP/1.1", 8) == 0)
    {
    version = 1;
    }
    else if(strncmp(msg, "HTTP/1.0", 8) == 0)
    {
    version = 1; // Handling this similar to version 1.1
    }
    else
    version = -1;

    return version;
}


void* thread_fn(void* socketNew) {
    // Waiting for the semaphore
    sem_wait(&semaphore);
    int semaphore_value;
    sem_getvalue(&semaphore, &semaphore_value);
    printf("🔐 Semaphore value before processing: %d\n", semaphore_value);

    int* t = (int*)(socketNew);
    int socket = *t;  // Socket descriptor of the connected client
    int bytes_sent_client, len;  // Bytes transferred

    // Create buffer for client communication
    char *buffer = (char*)calloc(MAX_BYTES, sizeof(char));  
    bzero(buffer, MAX_BYTES);  // Clear the buffer

    // Receive the client's request
    bytes_sent_client = recv(socket, buffer, MAX_BYTES, 0);  
    while (bytes_sent_client > 0) {
        len = strlen(buffer);
        // Loop until the "\r\n\r\n" sequence is found in the buffer (end of request)
        if (strstr(buffer, "\r\n\r\n") == NULL) {
            bytes_sent_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
        } else {
            break;
        }
    }
    

    printf("\n📜===============================================================📜\n");
    printf("📥 RECEIVED BUFFER CONTENT:\n\n%s\n\n", buffer);
    printf("\n📏====================== BUFFER LENGTH: %d ======================\n", (int)strlen(buffer));



    // Copy the HTTP request to `tempReq` for potential cache lookup
    char *tempReq = (char*)malloc(strlen(buffer) * sizeof(char) + 1);
    for (size_t i = 0; i < strlen(buffer); i++) {
        tempReq[i] = buffer[i];
    }

    // Check for the request in the cache
    struct cache_store* cached_response = find(tempReq);
    if (cached_response != NULL) {
        // Request found in cache, sending response from proxy's cache
        int cache_size = cached_response->len / sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];

        while (pos < cache_size) {
            bzero(response, MAX_BYTES);
            for (int i = 0; i < MAX_BYTES && pos < cache_size; i++) {
                response[i] = cached_response->data[pos++];
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("📦 Data retrieved from the Cache!\n\n%s\n\n", response);
    } 
    // If no cache and valid request
    else if (bytes_sent_client > 0) {
        len = strlen(buffer);
        ParsedRequest* request = ParsedRequest_create();

        // Parse the request, on success (returns 0) store it in 'request'
        if (ParsedRequest_parse(request, buffer, len) < 0) {
            printf("❌ Error: Failed to parse the request.\n");
        } else {
            bzero(buffer, MAX_BYTES);  // Clear the buffer

            if (!strcmp(request->method, "GET")) {  // Only supporting GET method
                // Ensure host, path, and valid HTTP version are present
                if (request->host && request->path && checkHTTPversion(request->version) == 1) {
                    bytes_sent_client = handle_request(socket, request, tempReq);  // Handle GET request
                    if (bytes_sent_client == -1) {
                        sendErrorMessage(socket, 500);  // Send 500 Internal Server Error
                    }
                } else {
                    sendErrorMessage(socket, 500);  // Invalid request, send 500 error
                }
            } else {
                printf("⚠️ This code only supports GET method. Received: %s\n", request->method);
            }
        }
        // Free the parsed request
        ParsedRequest_destroy(request);
    } 
    // Handling error in receiving request
    else if (bytes_sent_client < 0) {
        perror("❌ Error: Failed to receive data from client.\n");
    } 
    // Client disconnected
    else if (bytes_sent_client == 0) {
        printf("🔌 Client disconnected!\n");
    }

    // Clean up
    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);  // Release the semaphore

    // Check the semaphore value after processing
    sem_getvalue(&semaphore, &semaphore_value);
    printf("🔓 Semaphore value after processing: %d\n", semaphore_value);
    free(tempReq);
    return NULL;
}



int main(int argc, char* argv[]) {
    int client_socketID, client_len;
    struct sockaddr_in server_addr, client_addr;
    sem_init(&semaphore, 0, MAX_CLIENTS);

    pthread_mutex_init(&lock, NULL);  // using NULL because in C their exists garbage value by default
    if (argc == 2) {
        port_number = atoi(argv[1]);
        // ./proxy 8282 (for example)
        // so, here port_number = 8282 (for this particular case only)
    } else {
        printf("❌ Error: Insufficient arguments provided.\n");
        printf("💡 Tip: Try again with the correct arguments (e.g., ./proxy <port_number>)!\n");
        exit(1);
    }

    printf("🚀 Starting Proxy Server at Port: %d\n", port_number);
    proxy_socket_ID = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket_ID < 0) {
        perror("❌ Error: Failed to create a Socket.\n");
        exit(1);
    }

    int reuse = 1;
    if (setsockopt(proxy_socket_ID, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        perror("⚠️ Warning: setSockOpt failed.\n");
    }

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(proxy_socket_ID, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("❌ Error: Port is not available, please try another Port Number!!\n");
        exit(1);
    }
    printf("🔗 Binding on port %d... ✅\n", port_number);

    int listen_status = listen(proxy_socket_ID, MAX_CLIENTS);
    if (listen_status < 0) {
        perror("❌ Error: Listening failed.\n");
        exit(1);
    }

    int i = 0;
    int Connected_SocketID[MAX_CLIENTS];

    while (1) {
        bzero((char*)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketID = accept(proxy_socket_ID, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
        if (client_socketID < 0) {
            printf("❌ Error: Connection attempt failed.\n");
            exit(1);
        } else {
            Connected_SocketID[i] = client_socketID;
        }

        struct sockaddr_in* client_pt = (struct sockaddr_in*)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET6_ADDRSTRLEN);
        printf("✅ Client connected! Port: %d, IP: %s\n", ntohs(client_addr.sin_port), str);

        pthread_create(&t_id[i], NULL, thread_fn, (void*)&Connected_SocketID[i]);
        i++;
    }

    close(proxy_socket_ID);
    return 0;

}
