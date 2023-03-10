/* Creator: Michael Pasaye
 * Assignment: Web Proxy
 * Class: CSE 156 UCSC Winter 2023
 * Date: 03/09/23
*/

// INCLUDE STATEMENTS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// DEFINE STATEMENTS

// GLOBAL VARIABLES

// STRUCTS

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

    
}
