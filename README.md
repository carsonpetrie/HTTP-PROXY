# Asgn3 README

## Carson Petrie - cepetrie - cepetrie@ucsc.edu

## Directory Contents

* Makefile - Makefile to compile source code into executable
* httpproxy.c - Source code for implemenation of HTTP proxy. Holds main()
* httpproxy.h - Header file for httpproxy.c. Contains struct definitions
* cache.c - Source code for cacheQueue data structure as well as functions
* cache.h - Header file for cache library functions and struct definitons. 
* queue.h - Header file for queue.c
* queue.c - Source code for circular queue data structure implementation
* DESIGN.pdf - PDF file illustrating overall program design and implementation decisions
* WRITEUP.pdf - PDF file reflecting on code performance and testing
* README.md - Markdown file containing information on directory and code usage

## Executing and running httpserver.c

Build the executable by utilizing the `make` command. This will create an executable

file "httpproxy" that you can run with the linux command `./httpproxy`

.

The executable will only require one program argument, a port upon which to run the server.

Example format: `./httpserver <proxy_port> <server_port> ...` where all ports are valid integers 

representing localhost ports.

.

The executable will support the following optional arguments: `-N <connections>`, which will allow the

user to specify a number of threads for a program to utilize (must be > 0) and `-R <healthcheck>`, which

will specify a interval upon which our proxy should probe the provided servers. Caching is also supported with

the `-s <cache size>` command, which specifies the number of files to cache. `-m <cache space>` speciefies the

maximum size of our files to cache. Setting either of the caching commands to a value of 0 will disable caching.

.

The proxy can then be connected to in order to process the HTTP protocol requests for the Get command.

.

The makefile can also clean the directory via the `make clean` command. This will remove all

executable files, or .o files created during building.


