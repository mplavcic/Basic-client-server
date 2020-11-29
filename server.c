/*                      ***IMPORTANT STRUCTS***

    struct addrinfo {
        int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
        int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
        int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
        int              ai_protocol;  // use 0 for "any"
        size_t           ai_addrlen;   // size of ai_addr in bytes
        struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
        char            *ai_canonname; // full canonical hostname
    
        struct addrinfo *ai_next;      // linked list, next node
    };

    struct sockaddr {
        unsigned short    sa_family;    // address family, AF_xxx
        char              sa_data[14];  // 14 bytes of protocol address
    }; 

    struct sockaddr_in {
        short int          sin_family;  // Address family, AF_INET
        unsigned short int sin_port;    // Port number
        struct in_addr     sin_addr;    // Internet address
        unsigned char      sin_zero[8]; // Same size as struct sockaddr
    };

    struct in_addr {
        uint32_t s_addr; // that's a 32-bit int (4 bytes)
    };

    struct sockaddr_in6 {
        u_int16_t       sin6_family;   // address family, AF_INET6
        u_int16_t       sin6_port;     // port number, Network Byte Order
        u_int32_t       sin6_flowinfo; // IPv6 flow information
        struct in6_addr sin6_addr;     // IPv6 address
        u_int32_t       sin6_scope_id; // Scope ID
    };
    
    struct in6_addr {
        unsigned char   s6_addr[16];   // IPv6 address
    };

    struct sockaddr_storage {
        sa_family_t  ss_family;     // address family
    
        // all this is padding, implementation specific
        char      __ss_pad1[_SS_PAD1SIZE];
        int64_t   __ss_align;
        char      __ss_pad2[_SS_PAD2SIZE];
    };

    The header(signal.h) shall provide a declaration of struct sigaction, including at least the following members:
        
        void (*sa_handler)(int)  // pointer to a signal-catching function or one of the macros SIG_IGN or SIG_DFL 
        sigset_t sa_mask         // set of signals to be blocked during execution of the signal handling function 
        int      sa_flags        // special flags
        void (*sa_sigaction)(int, siginfo_t *, void *)     // pointer to a signal-catching function. 

*/

/* All this server does is send the string “Hello, world!” out over a stream connection. 
   All you need to do to test this server is run it in one window, 
   and telnet to it from another with:
   
         $ telnet remotehostname 3490
   
   where remotehostname is the name of the machine you’re running it on. */


   

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10 //max number of pending connections queue will hold

void sigchld_handler() {
    
    // waitpid() might overwrite errno, so we save and restore it
    int saved_errno = errno;
    
    // waitpid() system call suspends execution of the calling thread until a child specified by pid argument has changed state
    // -1 means wait for any child process whose process group ID is equal to the absolute value of pid
    // WNOHANG means return immediately if no child has exited
    // on success, returns the process ID of the child whose state has changed
    while(waitpid(-1, NULL, WNOHANG) > 0); 

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
    
    // get sockaddr, IPv4 or IPv6
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(void) {

    struct addrinfo hints, *res, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    int sockfd, newfd;
    int status;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue; // skips this iteration of the loop and continues with the next iteration
        }

        // getting rid of "Address already in use" error message
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;

    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {
        
        sin_size = sizeof(their_addr);
        newfd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof(s));
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            
            if (send(newfd, "Hello, world!", 13, 0) == -1)
                perror("send");
            
            close(newfd);
            exit(0);
        }
        
        close(newfd);  // parent doesn't need this
    }
    
    return 0;

}
