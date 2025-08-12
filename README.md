# Multithreaded-Proxy-Server in C

## Overview

This project implements a multi-threaded proxy server capable of handling multiple client requests in parallel, with an LRU (Least Recently Used) caching mechanism to store frequently accessed web pages. The proxy server manages concurrency through the use of semaphores and mutexes to ensure thread-safe operations.

---

## The aim of this project is to understand:

- How requests are sent from a local computer to a remote server.
- How to handle multiple client requests efficiently.
- How to manage concurrency using locking mechanisms.
- How caching works and the various functions that browsers might use.
- To explore system-level programming concepts such as multithreading, socket communication, and resource sharing.

---

## Proxy Server Functionality

- **Speed Enhancement:** The proxy server reduces the load on remote servers by caching frequently requested resources, speeding up the response time.
- **Security Features:** The proxy server can be extended to restrict access to certain websites or encrypt client requests to prevent unauthorized access.
- **IP Masking:** A proxy can mask the client's IP address, making it difficult for the server to identify the original request source.
  
---

## OS Components Used

- **Threading:** Utilized for handling multiple client connections concurrently.
- **Locks (Mutex):** Ensures that shared resources (like cache) are accessed safely without race conditions.
- **Semaphores:** Used to limit the number of simultaneous client connections.
- **Cache (LRU Algorithm):** Stores frequently accessed pages, evicting the least recently used entries when the cache is full.

---

## Limitations

- **Cache Duplication:** If a single URL opens multiple clients, the cache will store each response as a separate entry. This may result in incomplete data retrieval from the cache, causing websites to load incorrectly.
- **Fixed Cache Size:** Large websites may not be fully stored in the cache due to its fixed size limitation.
  
---

## How It Works

### Client Request Handling:

- The server listens for incoming client requests.
- Upon receiving a request, a new thread is spawned to handle it.
- The thread parses the HTTP request, forwards it to the remote server, and retrieves the response.

### Cache Management:

- When a response is retrieved from a remote server, it is stored in the cache (if the size is within limits).
- The **LRU (Least Recently Used)** algorithm is used to extract the least recently used cache entries when the cache exceeds its limit.
- Cache operations such as adding, finding, or removing entries are thread-safe, managed via a mutex lock.

### Concurrency Control:

- A semaphore is used to restrict the number of concurrent client connections.
- Mutex locks ensure that shared resources (like the cache) are not accessed concurrently by multiple threads.

### Error Handling:

- The server checks for common HTTP errors and sends appropriate error responses back to the client.

---

## Key Components

### 1. Multithreading:

- The server creates threads for each client request, using `pthread_create()`.
- Each thread is responsible for processing a request, forwarding it to the remote server, and responding back to the client.
- We used semaphores (`sem_wait()` and `sem_post()`) instead of `pthread_join()` for better control of concurrency without the need for thread-specific parameters.

### 2. LRU Cache Implementation:

- A custom cache structure (`cache_store`) is used to store responses from remote servers.
- The `find()` function looks for a cached response by URL, updating its **LRU timestamp** on access.
- If the cache is full, the `remove_cache_store()` function is called to remove the oldest entry.

### 3. Semaphores and Mutexes:

- The semaphore limits the number of concurrent threads, ensuring the server isn't overwhelmed by too many requests at once.
- A mutex lock (`pthread_mutex_lock`) ensures cache operations remain thread-safe, preventing race conditions during read/write operations.

---

## Functions and Logic 

### 1. Main Function (under "proxy_server.c" file):

- Initializes the server socket.
- Spawns threads to handle client requests.
- Uses a semaphore to control the number of active threads.

### 2. Cache Management Functions:

- `find(char* url)`: Searches for a URL in the cache.
- `add_cache_store(char* data, int size, char* url)`: Adds a new response to the cache.
- `remove_cache_store()`: Removes the least recently used cache entry when the cache exceeds its size limit.

### 3. Error Handling Function:

- `sendErrorMessage(int socket, int status_code)`: Sends appropriate HTTP error responses to the client.

### 4. Remote Server Communication:

- `connectRemoteServer(char* host_addr, int port_number)`: Establishes a connection to the remote server.
- `handle_request(int clientSocket, ParsedRequest *request, char *tempReq)`: Forwards the client request to the remote server and retrieves the response.

---

## How to Run

1. Clone the repository:

    ```bash
    $ git clone https://github.com/vanshajgupta37/Multithreaded-Proxy-Server
    ```

2. Navigate to the project directory:

    ```bash
    $ cd MultiThreaded-Proxy-Server
    ```

3. Compile the project:

    ```bash
    $ make all
    ```

4. Run the proxy server:

    ```bash
    $ ./cache_proxy <port no.> 
    ```

    Example:
    ```bash
    $ ./cache_proxy 8060
    ```

5. Open the proxy in your browser:

    ```bash
    Open http://localhost:<port>/https://www.cam.ac.uk/
    ```

    Example:
    ```bash
    Open http://localhost:8060/http://www.cam.ac.uk/
    ```

    Some more HTTP examples to try upon - 
    ```bash
    http://localhost:8060/http://www.testingmcafeesites.com/
    ```
    ```bash
    http://localhost:8060/http://www.archive.org
    ```
    ```bash
    http://localhost:8060/http://www.cs.washington.edu
    ```

**Note:** This code can only be run on a Linux machine.

---

### Video Tutorial to run the project: 
[![Watch the video](https://img.youtube.com/vi/11FcRm1qk6g/hqdefault.jpg)](https://youtu.be/11FcRm1qk6g?si=jygSZohk-bnjUeM7)

---

## Potential Enhancements

- **Multiprocessing:** The code can be extended to use multiprocessing for better parallelism and performance.
- **Website Filtering:** We can implement rules to restrict access to specific websites.
- **Additional HTTP Methods:** Support for `POST`, `PUT`, `DELETE`, and other HTTP methods could be added.
- **HTTPS Support:** Currently, only HTTP is supported. Implementing SSL/TLS would enable HTTPS requests.

---

## Known Issues

- **Concurrent Cache Access:** Even though cache operations are thread-safe, performance could be improved by optimizing cache access patterns or reducing contention on the mutex lock.
- **Fixed Cache Size:** Large websites may not fully fit in the cache, leading to partial caching and retrieval issues.

---

## Conclusion

This project demonstrates the ability to solve complex system-level programming challenges while implementing essential programming concepts. By incorporating multithreading, socket programming, cache management with LRU, and concurrency control using semaphores and mutexes, the proxy server is capable of efficiently handling multiple client requests in parallel.

The server addresses real-world problems related to resource management and concurrent processing, improving performance through a caching mechanism that reduces redundant network requests. This makes it suitable for applications requiring high throughput and low latency.

Additionally, the project serves as a solid foundation for further exploration of advanced networking concepts, performance optimization, and the integration of security features like SSL/TLS for HTTPS support.
