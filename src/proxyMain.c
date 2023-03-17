/* Creator: Michael Pasaye
 * Assignment: Web Proxy
 * Class: CSE 156 UCSC Winter 2023
 * Date: 03/09/23
*/

// INCLUDE STATEMENTS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
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
#define HTTP 7 // Count of characters for dest

// GLOBAL VARIABLES

// STRUCTS
struct tdinfo {
    int sfd;
    char* log;
    char* forb;
    SSL_CTX* ctx;
};

// HELPER FUNCTIONS
int reqParse (char *req, char* uri, int *port) {
    // Want to read Request-Line
    // to determine if valid method
    // and if host is not on forbidden list
    int i = 0;
    while ( req[i] != '\n' ) {
        i++;
    }
    char *reqln = (char*) malloc (i);
    memcpy (reqln, req, i);

    // Retrieve method
    i = 0;
    while ( req[i] != ' ' ) {
       i++; 
    }
    char *meth = (char*) malloc (i);
    memcpy (meth, reqln, i);

    // Retrieve URI
    i++;
    int j = i;

    while ( req[i] != ' ' ) {
        i++;
    }
    memcpy (uri, &(req[j + HTTP]), i - j - HTTP - 1);
    uri [i - j - HTTP - 1] = '\0';
    char *colon;
    colon = strrchr (&(uri[7]), ':');
    if ( colon != NULL ) {
        // Retrieve port
        i = 0;
        while ( colon[i] != '/' ) {
            i++;
        }
        char *portnum = (char*) malloc (i);
        memcpy (portnum, &(colon[1]), i - 1);
        *port = atoi (portnum);
    } else {
        *port = 443;
    }

    free (reqln);
    free (meth);
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
    int brcv;
    if ( (brcv = recv (targs -> sfd, req, MAXLINE, 0)) < 0 ) {
        fprintf (stderr, "Recieve error\n");
        close (targs -> sfd);
        pthread_cancel (pthread_self ());
    }

    printf ("%s", req);

    // Checking to see if method is supported
    char uri [64] = {0};
    int port;
    if ( reqParse (req, uri, &port) ) {
        // Need to send a a 501 HTTP Error
        close (targs -> sfd);
        pthread_cancel (pthread_self ());
    }

    // DNS lookup
    struct addrinfo hints, *res;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    int s = getaddrinfo (uri, "https", &hints, &res);
    if ( s != 0 ) {
        fprintf (stderr, "Failed to resolve IP address: %d\n", s);
        pthread_cancel (pthread_self ());
    }

    SSL* ssl = SSL_new (targs -> ctx);
    int sslfd;
    struct addrinfo *addr;
    for ( addr = res; addr != NULL; addr = addr -> ai_next ) {
        if ( (sslfd = socket (addr -> ai_family, addr -> ai_socktype, addr -> ai_protocol)) < 0 ) {
            continue;
        }

        if ( (SSL_set_fd (ssl, sslfd)) == 0 ) {
            continue;
        }

        if ( (connect (sslfd, addr -> ai_addr, addr -> ai_addrlen)) == 0 ) {
            if ( (SSL_connect (ssl)) == 1 ) {
                break;
            }
            break;
        }
    }
    freeaddrinfo (res);

    // Send request of SSL socket
    int bsnd;
    if ( (bsnd = SSL_write (ssl, req, brcv)) < 0 ) {
        fprintf (stderr, "Failed to send request over the ssl socket\n");
        pthread_cancel (pthread_self ());
    }

    char sslrep [MAXLINE] = {0};
    if ( (brcv = SSL_read (ssl, sslrep, MAXLINE)) < 0 ) {
        fprintf (stderr, "Failed to recieve over the ssl socket\n");
        pthread_cancel (pthread_self ());
    }
    
    // Need to send the content
    // of the response not the header
    
    if ( (bsnd = send (targs -> sfd, sslrep, bsnd, 0)) < 0 ) {
        fprintf (stderr, "Failed to send to the client\n");
        pthread_cancel (pthread_self ());
    }

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
