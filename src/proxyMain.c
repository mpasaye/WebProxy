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

// DEFINE STATEMENTS

// GLOBAL VARIABLES

// STRUCTS
struct tdinfo {
    int sfd;
    char* log;
    char* forb;
};

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

    // Need to set up listening port
    // to process HTTP requests I think
    // this can be a TCP socket

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

    // For loop to constantly process accept calls
    for (;;) {
        // Structure for connection
        struct sockaddr_in conn;
        socklen_t connlen = sizeof (conn);
        conn.sin_family = AF_INET;
        
        // Accepting incoming connectiong
        int connsfd = accept (listensfd, (struct sockaddr*) &conn, &connlen); 
        
        // Need to dispath thread to continue communication
        struct tdinfo tdata;
        tdata.sfd = connsfd;
        tdata.log = logpth;
        tdata.forb = forblst;
    }

}
