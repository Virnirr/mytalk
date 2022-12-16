#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>
#include <pwd.h>
#include <talk.h>

#define OPTMODENUM 6
#define MAXPORT 65535
#define PORTLEN 6
#define HOSTLEN 256
#define ANSWERLEN 4
#define UNAMESIZE 12
#define MAXBUFF 1000

/* Options for verbose flag on */
typedef struct Options {
    int opt_verbose;
    char opt_mode[OPTMODENUM];
    int opt_port;
    char *opt_host;
    int opt_accept;
    int opt_windows;
} Options;

void printf_option(Options *opt);
void parse_command_line(int argc, char *argv[], Options *option);
void server_chat(Options *opt);
void client_chat(Options *opt);
void ncurses_IO(int listener, Options *opt);

int main(int argc, char *argv[]) {
    /* initialize option */
    Options option = {0, "(none)", 0, "(none)", 0, 0};
    
    /* parses the command line and stores the options and information into
     * option structure. */
    parse_command_line(argc, argv, &option);

    if (!strcmp(option.opt_mode, "server")) 
        server_chat(&option);
    else 
        client_chat(&option);
    
    return 0;
}

void server_chat(Options *opt) {
    /* Takes in an option pointer that points to the options from 
     * command line and creates a socket binds with the local server 
     * and listens to any client that tries to connect with the socket. 
     * After getting a connection, the server will ask if it wants to 
     * connect with the client or no. If yes, then start the ncurses chat,
     * else, send a "no" message through the socket and end the connection. */

    struct sockaddr_in sa, client_addr;
    socklen_t client_len;
    int sockfd, newsock;
    char hostname[HOSTLEN], answer[ANSWERLEN], uname[UNAMESIZE];
    char peeraddr[INET_ADDRSTRLEN];
    
    sa.sin_family = AF_INET; /* IPv4 family */
    sa.sin_port = htons(opt -> opt_port); /* uses the input port */
    sa.sin_addr.s_addr = htonl(INADDR_ANY); /* all local interfaces */

    /* create socket in server to listen to any connections */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* bind the socket to an address */
    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* Listen to any incoming network. Only allow one network at a time */
    if (listen(sockfd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /* if verbose flag is on, print out the options. */
    if (opt -> opt_verbose) {
        printf_option(opt);
    }
    else 
        printf("Waiting for connection...");

    fflush(stdout);
    client_len = sizeof(client_addr);
    if ((newsock = accept(sockfd, 
        (struct sockaddr *)&client_addr, &client_len)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    memset(hostname, '\0', HOSTLEN);
    /* get hsot name from the client who is requesting */
    if (getnameinfo((struct sockaddr *)&client_addr, sizeof(struct sockaddr_in),
                    hostname, HOSTLEN, NULL, 0, NI_NAMEREQD) < 0) {
        perror("getnameinfo");
        exit(EXIT_FAILURE);
    }

    /* get the address family into a character string of the peer */
    if (!inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, 
            peeraddr, sizeof(peeraddr))){
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }

    printf("New connection from: %s:%d\n", peeraddr, 
            ntohs(client_addr.sin_port));

    /* receive user name from client */
    if (recv(newsock, uname, sizeof(uname), 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    
    uname[strlen(uname)] = '\0';

    /* if -a flag isn't on get the answer from the user x*/
    if (!opt -> opt_accept) {
        
        printf("Mytalk request from %s@%s.  Accept (y/n)? ", 
            uname, hostname);
        /* get answer if they want to connect or not */
        if (!fgets(answer, sizeof(answer), stdin)) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }
        /* if there exist a new line, then deleted it */
        if (answer[strlen(answer) - 1] == '\n') {
            answer[strlen(answer) - 1] = '\0';
        }
    }

    /* accept the connection if they said "yes" or "y" 
     * or if the a flag is on */
    if (!strncasecmp(answer, "yes", ANSWERLEN - 1) || 
        !strncasecmp(answer, "y", 1) || opt -> opt_accept) {
        /* send user name of the user */
        if (send(newsock, "ok", sizeof("ok"), 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        ncurses_IO(newsock, opt);
    }
    else {
        /* decline connection by sending a "no" */
        if (send(newsock, "no", sizeof("no"), 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        printf("VERB: declining connection\n");
    }
    /* all done, clean up */
    if (close(sockfd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    } 
    if (close(newsock) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
}

void client_chat(Options *opt) {
    /* Takes in an option pointer that points to the optinos from 
     * command line and creates a socket that tries to connect to the 
     * server and create a chat with the connection. It will send 
     * the client's username before asking to connect and be accepted. Then,
     * the client will wait for the server's response. If "ok", then the
     * client will start the ncurses chat until SIGINT. Else, the client will
     * close the connection and get refused if "no" is received. */
     
    struct addrinfo *host_info, host_info_hint;
    int sockfd, errnum;
    char port[PORTLEN], answer[ANSWERLEN];
    struct passwd *pwduid;


    /* create socket in server to listen to any connections */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* set up hints for host_info */
    host_info_hint.ai_family = AF_INET;
    host_info_hint.ai_socktype = SOCK_STREAM;
    host_info_hint.ai_protocol = 0;
    host_info_hint.ai_flags = AI_CANONNAME;

    /* get the address info of the host from the command line */
    memset(port, '\0', PORTLEN);
    sprintf(port, "%d", opt -> opt_port);
    if ((errnum = getaddrinfo(opt -> opt_host, port, 
            &host_info_hint, &host_info)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errnum));
        exit(EXIT_FAILURE);
    }

    /* if verbose flag is on, print out the options. */
    if (opt -> opt_verbose)
        printf_option(opt);
    else 
        printf("Waiting for response from %s\n", opt -> opt_host);

    /* connect to host */
    if ((connect(sockfd, host_info -> ai_addr, host_info -> ai_addrlen)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    /* Grab uname from UID */
    if (!(pwduid = getpwuid(getuid()))) {
        perror("getpwduid");
        exit(EXIT_FAILURE);
    }

    /* send user name of the user */
    if (send(sockfd, pwduid -> pw_name, strlen(pwduid -> pw_name), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    /* receive user name from client */
    if (recv(sockfd, answer, sizeof(answer), 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    if (!strncasecmp(answer, "ok", 2)) {
        ncurses_IO(sockfd, opt);
    }
    else {
        printf("%s declined connection\n", opt -> opt_host);
    }

    /* close up the socket and clena up */
    /* all done, clean up */
    if (close(sockfd) < 0 ) {
        perror("close");
        exit(EXIT_FAILURE);
    } 
    freeaddrinfo(host_info);
}

void ncurses_IO(int listener, Options *opt) {
    char buff[MAXBUFF];
    struct pollfd fds[2];
    int num_read, byte_recv;

    /* set up the pollfd to poll for listening to data from network */
    fds[0].fd = listener; /* socket of peer */
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    /* set up poll for stdin to write data to the peer */
    fds[1].fd = 0; /* stdin fd */
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    /* open window for ncurses*/
    if (!(opt -> opt_windows))
        start_windowing();


    do {
        /* Keep polling and checking if you received any read data from the 
         * socket. Or have written any data */
        if (poll(fds, 2, -1) < 0) {
            /* closing window for ncurses */
            if (!(opt -> opt_windows))
                stop_windowing();
            perror("poll");
            exit(EXIT_FAILURE);
        }

        /* If you received any data, write it to stdout */
        if ((fds[0].revents & POLLIN)) {
            /* receive user name from client */
            if ((byte_recv = recv(listener, buff, MAXBUFF, 0)) <= 0) {
                /* if the byte is 0, then write the connection closed */
                if (byte_recv == 0) {
                    if (
                    write_to_output("\nConnection closed. ^C to terminate.\n", 
                    strlen("\nConnection closed. ^C to terminate.\n")) == ERR) {
                        /* closing window for ncurses */
                        if (!(opt -> opt_windows))
                            stop_windowing();
                    
                        perror("write_to_output");
                        exit(EXIT_FAILURE);
                    }
                    pause(); /* pause the process when terminate */
                }
                /* else error out */
                else {
                    /* closing window for ncurses */
                    if (!(opt -> opt_windows))
                        stop_windowing();
                    perror("recv");
                    exit(EXIT_FAILURE);
                }
            }
            else {
                if (write_to_output(buff, byte_recv) == ERR) {
                    /* closing window for ncurses */
                    if (!(opt -> opt_windows))
                        stop_windowing();
                    perror("write_to_output");
                    exit(EXIT_FAILURE);
                }
            }
            memset(buff, '\0', MAXBUFF);
        }
        
        /* check to see if you typed a new line and if it has a newline 
         * aka your stdin is buffered and ready to be put into buffer */
        if ((fds[1].revents & POLLIN)) {
            /* update the internal buffer with the stdin */
            update_input_buffer();
            /* check if the buffer is an entire line (i.e. if it has a
             * new line or EOF at the end) */
            if(has_whole_line()){
                /* read into buff if it is a new line and send to 
                 * the peer */
                if ((num_read = read_from_input(buff, MAXBUFF)) == ERR) {
                    /* closing window for ncurses */
                    if (!(opt -> opt_windows))
                        stop_windowing();
                    perror("read_from_input");
                    exit(EXIT_FAILURE);
                }
                if (send(listener, buff, num_read, 0) < 0) {
                    /* closing window for ncurses */
                    if (!(opt -> opt_windows))
                        stop_windowing();
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
        }

    } while (!has_hit_eof());
    
    /* closing window for ncurses */
    if (!(opt -> opt_windows))
        stop_windowing();
}


void parse_command_line(int argc, char *argv[], Options *option) {
    
    int opt, port;
    char *num;

    /* get each flag into the option structure */
    while ((opt = getopt(argc, argv, "vaN")) != -1) {
        switch (opt) {
            /* checks for v flag (verbose) */
            case 'v':
                option -> opt_verbose++;
                break;
            /* checks for a flag 
             * -a (server) accept a connection 
             * without asking (useful while debugging)*/
            case 'a':
                option -> opt_accept++;
                break;
            /* checks: 
             * -N do not start ncurses windowing (useful while debugging)*/
            case 'N':
                option -> opt_windows++;
                break;
            /* flag is not what we're looking for, then error out */
            default:
                fprintf(stderr, "unknown option: -%c\n", opt);
                fprintf(stderr, 
                    "usage: mytalk [ -v ] [ -a ] [ -N ] [ hostname ] port\n");
                exit(EXIT_FAILURE);
        }
    }
    /* Check if it's only one argument after flag. If it is, then it's host */
    if (optind == (argc - 1)) {
        /* set the opt_mode as server since only port number is set*/
        strcpy(option -> opt_mode, "server");
        num = argv[optind];

        /* check if the port number is a valid number */
        while (*num) {
            if (!isdigit(*num)) {
                fprintf(stderr, "%s: badly formed port number\n", argv[optind]);
                exit(EXIT_FAILURE);
            }
            num++;
        }
        
        port = atoi(argv[optind]);
        option -> opt_port = port;
        /* if the port is greater than 65535, error out */
        if (port > MAXPORT || port < 1) {
            fprintf(stderr, 
                    "usage: mytalk [ -v ] [ -a ] [ -N ] [ hostname ] port\n");
            exit(EXIT_FAILURE);
        }
    }

    /* if there are host and port number, then it's client */
    else if (optind == (argc - 2)) {

        strcpy(option -> opt_mode, "client");
        num = argv[optind + 1]; /* get the port number */
        option -> opt_host = argv[optind]; /* get the host name */

        /* check if the port number is a valid number */
        while (*num) {
            if (!isdigit(*num)) {
                fprintf(stderr, "%s: badly formed port number\n", 
                    argv[optind + 1]);
                exit(EXIT_FAILURE);
            }
            num++;
        }
        
        port = atoi(argv[argc - 1]);
        option -> opt_port = port;
        
        /* if the port is greater than 65535, error out */
        if (port > MAXPORT || port < 1) {
            fprintf(stderr, 
                    "usage: mytalk [ -v ] [ -a ] [ -N ] [ hostname ] port\n");
            exit(EXIT_FAILURE);
        }
    }
    /* else it's a bad usage */
    else {
        fprintf(stderr, 
            "usage: mytalk [ -v ] [ -a ] [ -N ] [ hostname ] port\n");
        exit(EXIT_FAILURE);
    }
}

void printf_option(Options *opt) {
    printf("Options:\n");
    printf("  %s %16s %1s %d\n", "int", "opt_verbose", "=", opt -> opt_verbose);
    printf("  %s %8s %4s %s\n", "talkmode", "opt_mode", "=", opt -> opt_mode);
    printf("  %s %13s %4s %d\n", "int", "opt_port", "=", opt -> opt_port);
    printf("  %s %12s %4s %s\n", "char", "*opt_host", "=", opt -> opt_host);
    printf("  %s %14s %3s %d\n", "int", "opt_accept", "=", opt -> opt_accept);
    printf("  %s %16s %1s %d\n", "int", "opt_windows", "=", opt -> opt_windows);
    if (!strcmp(opt -> opt_mode, "server")) 
        printf("VERB: Waiting for connection...");
    else 
        printf("Waiting for response from %s\n", opt -> opt_host);
}