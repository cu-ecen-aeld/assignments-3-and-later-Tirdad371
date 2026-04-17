/*
 * aesdsocket.c
 * A stream socket server on port 9000.
 * Receives newline-delimited packets, appends to /var/tmp/aesdsocketdata,
 * and echoes the full file back to the client after each complete packet.
 *
 * Usage:  ./aesdsocket        (foreground)
 *         ./aesdsocket -d     (daemon mode - Part 2)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */
#define PORT          9000
#define DATAFILE      "/var/tmp/aesdsocketdata"
#define BACKLOG       10          /* max pending connections            */
#define RECV_CHUNK    1024        /* bytes read from socket at a time   */
#define SEND_CHUNK    4096        /* bytes read from file per send()    */

/* ------------------------------------------------------------------ */
/*  Globals touched by the signal handler                              */
/* ------------------------------------------------------------------ */
static volatile sig_atomic_t g_exit_requested = 0;
static int  g_server_fd  = -1;   /* listening socket                   */
static int  g_client_fd  = -1;   /* currently active client socket     */

/* ------------------------------------------------------------------ */
/*  Signal handler                                                      */
/* ------------------------------------------------------------------ */
static void signal_handler(int signo)
{
    (void)signo;                  /* both SIGINT and SIGTERM do the same */
    g_exit_requested = 1;

    /*
     * Wake up any blocking accept() / recv() by shutting down the
     * sockets.  The main loop will detect g_exit_requested and clean up.
     */
    if (g_client_fd != -1)  shutdown(g_client_fd, SHUT_RDWR);
    if (g_server_fd != -1)  shutdown(g_server_fd, SHUT_RDWR);
}

/* ------------------------------------------------------------------ */
/*  Register SIGINT and SIGTERM                                         */
/* ------------------------------------------------------------------ */
static int setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    /* SA_RESTART is intentionally NOT set so that blocking calls return
       EINTR when a signal arrives, letting us exit the loop cleanly.   */

    if (sigaction(SIGINT,  &sa, NULL) == -1) return -1;
    if (sigaction(SIGTERM, &sa, NULL) == -1) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Create, bind, and listen on port PORT                              */
/*  Returns the server fd on success, -1 on any failure.              */
/* ------------------------------------------------------------------ */
static int create_server_socket(void)
{
    int fd;
    int opt = 1;
    struct sockaddr_in addr;

    /* 1. Create TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        return -1;
    }

    /* 2. Allow rapid restart without "Address already in use" */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* 3. Bind to 0.0.0.0:9000 */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* 4. Start listening */
    if (listen(fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/* ------------------------------------------------------------------ */
/*  Handle one connected client                                         */
/*                                                                      */
/*  - Receive bytes until a '\n' is found  → one complete packet.      */
/*  - Append the complete packet to DATAFILE.                          */
/*  - Send the entire content of DATAFILE back to the client.          */
/*  - Repeat until the client closes the connection.                   */
/*                                                                      */
/*  Returns  0 on clean close, -1 on error.                           */
/* ------------------------------------------------------------------ */
static int handle_client(int client_fd)
{
    int    ret        = 0;

    /* --- dynamic receive buffer --- */
    char  *pkt_buf   = NULL;   /* current packet being assembled       */
    size_t pkt_len   = 0;      /* bytes stored in pkt_buf              */
    size_t pkt_cap   = 0;      /* allocated capacity of pkt_buf        */

    /* --- small stack buffer for recv() --- */
    char   recv_buf[RECV_CHUNK];

    while (!g_exit_requested) {

        /* ---- receive a chunk from the client ---- */
        ssize_t n = recv(client_fd, recv_buf, sizeof(recv_buf), 0);

        if (n == -1) {
            if (errno == EINTR) break;   /* signal received */
            syslog(LOG_ERR, "recv() failed: %s", strerror(errno));
            ret = -1;
            break;
        }
        if (n == 0) break;   /* client closed connection */

        /* ---- append received bytes to the packet buffer ---- */
        /* Grow the buffer if needed */
        if (pkt_len + (size_t)n > pkt_cap) {
            size_t new_cap = pkt_cap + (size_t)n + RECV_CHUNK;
            char  *tmp     = realloc(pkt_buf, new_cap);
            if (!tmp) {
                syslog(LOG_ERR, "realloc() failed – discarding packet");
                free(pkt_buf);
                pkt_buf = NULL;
                pkt_len = 0;
                pkt_cap = 0;
                ret = -1;
                break;
            }
            pkt_buf = tmp;
            pkt_cap = new_cap;
        }
        memcpy(pkt_buf + pkt_len, recv_buf, (size_t)n);
        pkt_len += (size_t)n;

        /*
         * Process all complete packets in the buffer.
         * A packet is complete when we find a '\n'.
         * There may be more than one '\n' in what we received.
         */
        char *search_start = pkt_buf;          /* where to look for '\n' */

        while (1) {
            /* Find the next newline in the assembled buffer */
            char *nl = memchr(search_start, '\n',
                              pkt_buf + pkt_len - search_start);
            if (!nl) break;   /* no complete packet yet */

            size_t packet_bytes = (size_t)(nl - pkt_buf) + 1; /* include '\n' */

            /* ---- append complete packet to the data file ---- */
            int data_fd = open(DATAFILE,
                               O_WRONLY | O_CREAT | O_APPEND,
                               0644);
            if (data_fd == -1) {
                syslog(LOG_ERR, "open(%s) failed: %s", DATAFILE, strerror(errno));
                ret = -1;
                goto cleanup;
            }

            ssize_t written = write(data_fd, pkt_buf, packet_bytes);
            close(data_fd);

            if (written != (ssize_t)packet_bytes) {
                syslog(LOG_ERR, "write() to data file incomplete");
                ret = -1;
                goto cleanup;
            }

            /* ---- send entire file content back to client ---- */
            data_fd = open(DATAFILE, O_RDONLY);
            if (data_fd == -1) {
                syslog(LOG_ERR, "open(%s) for read failed: %s",
                       DATAFILE, strerror(errno));
                ret = -1;
                goto cleanup;
            }

            char  send_buf[SEND_CHUNK];
            ssize_t bytes_read;
            while ((bytes_read = read(data_fd, send_buf, sizeof(send_buf))) > 0) {
                ssize_t total_sent = 0;
                while (total_sent < bytes_read) {
                    ssize_t sent = send(client_fd,
                                       send_buf + total_sent,
                                       (size_t)(bytes_read - total_sent),
                                       0);
                    if (sent == -1) {
                        if (errno == EINTR) continue;
                        syslog(LOG_ERR, "send() failed: %s", strerror(errno));
                        close(data_fd);
                        ret = -1;
                        goto cleanup;
                    }
                    total_sent += sent;
                }
            }
            close(data_fd);

            /* ---- slide remaining bytes to front of packet buffer ---- */
            size_t remaining = pkt_len - packet_bytes;
            if (remaining > 0)
                memmove(pkt_buf, pkt_buf + packet_bytes, remaining);
            pkt_len    -= packet_bytes;
            search_start = pkt_buf;   /* restart search from beginning */
        }
    } /* while (!g_exit_requested) */

cleanup:
    free(pkt_buf);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  Cleanup: close sockets and remove the data file                   */
/* ------------------------------------------------------------------ */
static void cleanup(void)
{
    if (g_client_fd != -1) { close(g_client_fd); g_client_fd = -1; }
    if (g_server_fd != -1) { close(g_server_fd); g_server_fd = -1; }
    remove(DATAFILE);
    syslog(LOG_INFO, "Caught signal, exiting");
    closelog();
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/*  Daemonize: bind FIRST, fork SECOND                                 */
/* ------------------------------------------------------------------ */
static int daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork() failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) exit(EXIT_SUCCESS);   /* parent exits */

    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        return -1;
    }
    if (chdir("/") == -1) {
        syslog(LOG_ERR, "chdir(/) failed: %s", strerror(errno));
        return -1;
    }

    int devnull = open("/dev/null", O_RDWR);
    if (devnull == -1) return -1;
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    if (devnull > 2) close(devnull);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    int daemon_mode = (argc == 2 && strcmp(argv[1], "-d") == 0);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (setup_signals() == -1) {
        syslog(LOG_ERR, "Failed to set up signal handlers");
        return -1;
    }

    /* Bind BEFORE fork so we fail cleanly if port is unavailable */
    g_server_fd = create_server_socket();
    if (g_server_fd == -1) return -1;

    if (daemon_mode && daemonize() == -1) {
        close(g_server_fd);
        return -1;
    }

    /* ---- accept loop ---- */
    while (!g_exit_requested) {

        struct sockaddr_in client_addr;
        socklen_t          addrlen = sizeof(client_addr);

        g_client_fd = accept(g_server_fd,
                             (struct sockaddr *)&client_addr,
                             &addrlen);
        if (g_client_fd == -1) {
            if (errno == EINTR) break;   /* signal woke us up */
            syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
            continue;
        }

        /* Convert client IP to string for logging */
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));

        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        handle_client(g_client_fd);

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(g_client_fd);
        g_client_fd = -1;
    }

    cleanup();
    return 0;
}

