# Multi Threaded Web Proxy Server with LRU Cache

This project has been implemented utlising C programming langugaes, Data Structures and HTTP parsing library from Proxy Server.

## Table of Contents
- [Key Features](#overview)
- [How It Works](#how-it-works)

## UML Diagram:

![image](https://github.com/thejediboySHASHANK/WebServerLRU/assets/95047201/8ea4e464-c404-46f0-9859-f39e42511cad)

# Overview

**WebServerLRU** is a robust, multi-threaded web proxy server implemented in C. It leverages the POSIX threads (pthreads) library to manage multiple client requests concurrently, ensuring efficient handling of network traffic. The server integrates key concepts from operating systems such as semaphores and mutexes to securely manage concurrent access to shared resources.

## Key Features

- **Multi-Threading**: Utilizes the pthreads library to enable simultaneous processing of multiple client requests.
- **Concurrency Control**: Employs mutexes and semaphores to ensure safe access to shared resources among threads.
- **HTTP Request Handling**: Each incoming connection triggers the creation of a new thread that processes the client's HTTP request using the `proxy_parse.h` library, and forwards it to the intended remote server.
- **Response Caching**: Implements an LRU (Least Recently Used) caching mechanism to optimize response times. The server checks this cache before forwarding requests to the remote server, reducing latency and network load.
- **Socket Programming**: Uses standard socket programming functions such as `socket()`, `bind()`, `listen()`, and `accept()` for server setup. Data transmission is handled through `send()` and `recv()` functions.
- **DNS Resolution**: Incorporates `gethostbyname()` to resolve hostnames to IP addresses, facilitating the handling of HTTP requests to various domains.

## How It Works

Upon launching, the server begins listening for incoming TCP connections. When a connection is established, it accepts the connection and spins off a new thread dedicated to handling that specific client. Each thread performs the following tasks:
- Parses the client’s HTTP request.
- Checks the cache for a stored response.
- If cached, returns the response directly to the client.
- If not cached, forwards the request to the designated remote server, retrieves the response, sends it back to the client, and stores it in the cache.

This architecture not only enhances the responsiveness of the proxy server but also significantly reduces unnecessary network traffic by serving repeated requests directly from the cache.
