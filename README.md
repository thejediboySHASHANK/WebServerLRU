# Multi Threaded Web Proxy Server with LRU Cache

This project has been implemented utlising C programming langugaes, Data Structures and HTTP parsing library from Proxy Server

## UML Diagram:

![image](https://github.com/thejediboySHASHANK/WebServerLRU/assets/95047201/8ea4e464-c404-46f0-9859-f39e42511cad)

## Project Overview

This C program is a multi-threaded proxy server with caching capabilities. It uses the POSIX threads (pthreads) library for multi-threading, allowing it to handle multiple client requests concurrently. The server uses semaphores and mutexes to manage concurrent access to shared resources, which are key concepts in operating systems.  

The server listens for incoming connections, accepts them, and creates a new thread to handle each connection. Each thread receives the client's HTTP request, parses it using the provided proxy_parse.h library, and forwards it to the appropriate remote server. The response from the remote server is then sent back to the client.  

The program also implements a cache to store responses. When a request is received, the server first checks if the response is already in the cache. If it is, the cached response is sent to the client. If not, the request is forwarded to the remote server, and the response is stored in the cache for future use. The cache uses a Least Recently Used (LRU) policy for cache replacement, which is implemented by tracking the access time of each cache element.  

The server uses standard socket programming techniques in C, including the socket(), bind(), listen(), and accept() functions for setting up the server, and the send() and recv() functions for sending and receiving data. It also uses the gethostbyname() function to resolve hostnames to IP addresses
