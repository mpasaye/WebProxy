/* Creator: Michael Pasaye
 * Assignment: Web Proxy
 * Class: CSE 156 UCSC Winter 2023
 * Date: 03/09/23
*/

// INCLUDE STATEMENTS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// DEFINE STATEMENTS
#define MAXLINE 1024

// GLOBAL VARIABLES

// STRUCTS
struct tdinfo {
    int sfd;
    char* log;
    char* forb;
};

// THREAD FUNCTION
void *cliwrk (void* args) {
    struct tdinfo *targs = (struct tdinfo*) args; 
    
    // Read the data from the socket
    char msg [MAXLINE];
    if ( (recv (targs -> sfd, msg, MAXLINE, 0)) < 0 ) {
        fprintf (stderr, "Recieve error\n");
        return 0;
    }
    fprintf (stderr, "recv: %s\n", msg);

    return 0;
}

int main (int argc, char** argv) {
    int i = argc - 1;

    // Copying over the path to the log file
    char* logpth = (char*) malloc (strlen (argv[i]) + 1);
    memcpy (logpth, argv[i], strlen (argv[i]) + 1);
    
    // Copying over the path to forbidden sites list
    char* forblst = (char*) malloc (strlen (argv[i-1]) + 1);
    memcpy (forblst, argv[i-1], strlen (argv[i-1]) + 1);

    // Copying over listening port
    int port = 0;
    if ( (strcmp ("0", argv[i-2])) != 0 ) {
        // Port argument was not 0
        port = strtol (argv[i-2], NULL, 10);
        if (!port) {
            fprintf (stderr, "Could not convert port argument to digit\n");
            exit (1);
        }
    }

    // Creating struct for listening socket
    struct sockaddr_in local;
    socklen_t locallen = sizeof (local);
    local.sin_family = AF_INET; // Using IPV4 & IPV6
    local.sin_port = htons (port);
    local.sin_addr.s_addr = htonl (INADDR_ANY); // Binding on all interfaces

    // Creating listening socket using TCP
    int listensfd;
    if ( (listensfd = socket (AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf (stderr, "Failed to create listening socket\n");
        exit (1);
    }

    // Binding socket to local address and port
    if ( (bind (listensfd, (struct sockaddr*) &local, locallen)) < 0 ) {
        fprintf (stderr, "Failed to bind socket to local address\n");
        exit (1);
    }

    // Ensure the socket is listening
    if ( (listen (listensfd, 128)) < 0 ) {
        fprintf (stderr, "Failed to listen on socket\n");
        exit (1);
    }

    // Poll struct to catch when accept has an event
    struct pollfd polls[1];
    polls[0].fd = listensfd;
    polls[0].events = POLLIN; // Want to wait till there is data from accept

    // Initializing SSL Library
    SSL_library_init ();

    // For loop to constantly process accept calls
    for (;;) {
        // Watching for events
        if ( (poll (polls, 1, -1)) < 0 ) {
            fprintf (stderr, "Poll had an error\n");
            close (listensfd);
            exit (1);
        }

        // Poll found an event
        if (polls[0].revents & POLLIN) {
            // Structure for client
            struct sockaddr_in cli;
            socklen_t clilen = sizeof (cli);
            cli.sin_family= AF_INET;

            // Accepting new connection
            int connsfd = accept (listensfd, (struct sockaddr*) &cli, &clilen);

            // Create argument for thread
            struct tdinfo tdata;
            tdata.sfd = connsfd;
            tdata.log = logpth;
            tdata.forb = forblst;

            // Creating and Deploying thread
            pthread_t td;
            if ( (pthread_create (&td, NULL, &cliwrk, (void*) &tdata)) < 0 ) {
                fprintf (stderr, "Error creating thread\n");
                exit (1);
            } pthread_detach (td);
        }
    }
}
