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
    SSL_CTX* ctx;
};

// HELPER FUNCTIONS
int reqParse (char *req, char *meth, char *host, int *port) {
    // Want to read Request-Line
    // to determine if valid method
    // and if host is not on forbidden list
    int i = 0;
    while ( req[i] != '\n' ) {
        i++;
    }

    char *reqln = (char*) malloc (i + 1);
    memcpy (reqln, req, i);
    
    i = 0;
    while ( reqln[i] != ' ' ) {
        i++;
    }
    memcpy (meth, reqln, i);

    // Checking if method is valid
    if ( strcmp (meth, "GET") ) {
        printf ("method was not get: %s\n", meth);
        if ( strcmp (meth, "HEAD") ) {
            printf ("method was not head: %s\n", meth);
            free (meth);
            free (reqln);
            return 1; 
        }
    }

    // Retrieving Request-URI
    i += 1;
    int j = i;
    while ( req[i] != ' ' ) {
        i++;
    } i = i-j;
    char *uri = (char*) malloc (i);
    memcpy (uri, &(req[j]), i);

    // Find port
    char *ptr = strrchr (&(uri[7]), ':');
    if ( ptr == NULL ) {
        memcpy (host, uri, strlen (uri));
        *port = 443; 
    } else {
        i = 0;
        while ( ptr[i] != '/' ) {
            i++;
        }
        char *num = (char*) malloc (i);
        memcpy (num, &(ptr[1]), i - 1);
        *port = atoi (num);

        // Need to remove the port from the uri
        i = 7;
        while ( uri [i] != ':' ) {
            i++;
        }
        memcpy (host, uri, i + 7);

        free (num);
    }

    free (uri);
    free (reqln);
    return 0; 
}

// THREAD FUNCTION
void *cliwrk (void* args) {
    // Thread should not close listening socket
    // but for now, I will do this so the port is
    // freed during testing
    
    struct tdinfo *targs = (struct tdinfo*) args; 
    
    // Read initial request from client
    char req [MAXLINE];
    if ( (recv (targs -> sfd, req, MAXLINE, 0)) < 0 ) {
        fprintf (stderr, "Recieve error\n");
        close (targs -> sfd);
        return 0;
    }

    printf ("%s", req);

    // Checking to see if method is supported
    char uri [64] = {0};
    char meth [10] = {0};
    int port;
    if ( reqParse (req, meth, uri, &port) ) {
        // Need to send a a 501 HTTP Error
        close (targs -> sfd);
        return 0;
    }

    printf ("meth: %s\nuri: %s\nport: %d\n", meth, uri, port);
    // New HTTP Request
    char nreq [128] = {0};
    sprintf (nreq, "%s %s:%d HTTP/1.1\r\n\r\n", meth, uri, port);
    printf ("nreq: %s", nreq);

    // Creating SSL socket to forward HTTP request
    SSL* ssl = SSL_new (targs -> ctx);
    int sslfd; 
    
    // Creating socket
    if ( (sslfd = socket (AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf (stderr, "Failure creating ssl socket\n");
        return 0;
    }

    // Attaching SSL object to socket
    SSL_set_fd (ssl, sslfd);

    close (targs -> sfd);
    close (sslfd);
    return 0;
}

// MAIN FUNCTIONS
int main (int argc, char** argv) {
    if (argc < 4) {
        fprintf (stderr, "Missing arguments\n");
        exit (1);
    } else if (argc > 4) {
        fprintf (stderr, "Too many arguments\n");
        exit (1);
    }

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
    SSL_load_error_strings ();
    const SSL_METHOD* meth = SSLv23_client_method ();
    SSL_CTX* ctx = SSL_CTX_new (meth);

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
            tdata.ctx = ctx;

            // Creating and Deploying thread
            pthread_t td;
            if ( (pthread_create (&td, NULL, &cliwrk, (void*) &tdata)) < 0 ) {
                fprintf (stderr, "Error creating thread\n");
                close (listensfd);
                close (connsfd);
                exit (1);
            } pthread_detach (td);
        }
    }
}
