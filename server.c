#include <stdio.h> //printf, perror, FILE, fopen, fprintf
#include <stdlib.h> //malloc, free, exit
#include <string.h> //strlen, strcpy, strcmp, strncpy, strcat
#include <unistd.h> //close(), read(), write()
#include <errno.h> //errno, EINTR
#include <arpa/inet.h> //htons, inet_ntoa, ntohs
#include <netinet/in.h> //sockaddr_in, INADDR_ANY
#include <sys/socket.h> //socket(), bind(), listen(), accept(), send(), recv()
#include <pthread.h> //pthread_t, pthread_create, mutex
#include <time.h> //time(), localtime_r(), strftime()
#include <stdarg.h> //for va_list, va_start, and va_end

#include "communication.h" //custom header to define shared constants PORT, MAX_CLIENTS, BUFFER_SIZE

// Data structure for connected client. This serves as the in-memory record for each connected user. 
typedef struct {
    int sock; //socket descriptor
    char name[32]; //client's username, set by user
    struct sockaddr_in addr; //client's network address info
    int in_use; //slot is active or not. 1=active, 0=free
} Client;
//global variables
static Client clients[MAX_CLIENTS]; //array to store connected clients
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; //protects the clients array
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; //protects log file access

//Utility, gets current timestamp (HH:MM:SS)
static void ts_now(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm); //thread-safe version of localtime()
    strftime(out, n, "%H:%M:%S", &tm); //formatting into out buffer
}
//Appends a new line to the server log, server.log, (with timestamp)
static void log_line(const char *line) {
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen("server.log", "a");
    if (fp) {
        char ts[16];
        ts_now(ts, sizeof ts);
        fprintf(fp, "[%s] %s\n", ts, line);
        fclose(fp);
    }
    pthread_mutex_unlock(&log_mutex);
}

//Sends entire buffer to socket, handles partial sends
static int send_all(int sock, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue; //retry in case of signal interruption
            return -1;
        }
        if (n == 0) return -1; //closed connection
        sent += (size_t)n;
    }
    return 0;
}

//Sends null-terminated C string over TCP socket. Helps when sending messages without specifiying size in caller
static int send_str(int sock, const char *s) {
    return send_all(sock, s, strlen(s));
}

//sends message to all connected clients except the skip_sock

static void broadcast(const char *msg, int skip_sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].in_use && clients[i].sock != skip_sock) {
            send_str(clients[i].sock, msg);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//finds client index by name. Used for PMs
static int find_by_name(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].in_use && strcmp(clients[i].name, name) == 0) return i;
    }
    return -1;
}

//adds new client to clients array, new user enters the chat
static int add_client(int sock, struct sockaddr_in *addr) {
    pthread_mutex_lock(&clients_mutex);
    int idx = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i].in_use) { idx = i; break; }
    }
    if (idx != -1) {
        clients[idx].sock = sock;
        clients[idx].addr = *addr;
        clients[idx].name[0] = '\0';
        clients[idx].in_use = 1;
    }
    pthread_mutex_unlock(&clients_mutex);
    return idx;
}

//removes client from list, optionally returning their name, calls close() on socket
static void remove_client(int sock, char *name_out) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].in_use && clients[i].sock == sock) {
            if (name_out) strncpy(name_out, clients[i].name, sizeof(clients[i].name) - 1);
            clients[i].in_use = 0;
            close(clients[i].sock);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//strips trailing new lines from string

static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

//Broadcasts system message to all clients and logs it. (For example a message notifying chat of a new user). Auto appends newline to message
static void system_broadcast(const char *fmt, ...) {
    char msg[BUFFER_SIZE * 2];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    strcat(msg, "\n");
    broadcast(msg, -1);
    log_line(msg);
}
//Thread function for each connected client. ##################################
static void *client_thread(void *arg) {
    int sock = *(int*)arg;
    free(arg);

    char buf[BUFFER_SIZE];
    char name[32] = {0};
//asks for username
    send_str(sock, "Enter username: ");
    ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(sock); return NULL; }
    buf[n] = '\0';
    rstrip(buf);
    if (buf[0] == '\0') snprintf(buf, sizeof buf, "user%d", sock % 10000);
//checks if username is unique
    pthread_mutex_lock(&clients_mutex);
    char base[32]; strncpy(base, buf, sizeof base);
    int suffix = 1;
    while (find_by_name(buf) != -1) {
        snprintf(buf, sizeof buf, "%s_%d", base, suffix++);
    }
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].in_use && clients[i].sock == sock) {
            strncpy(clients[i].name, buf, sizeof(clients[i].name)-1);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    strncpy(name, buf, sizeof name - 1);

//announce join
    system_broadcast("[system] %s joined the chat.", name);
    send_str(sock, "Type /who, /nick <newname>, /msg <user> <text>, or /quit\n");

//Main loop: handles client messages. command parser for /who, /nick, /msg/ and /quit
    while (1) {
        n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n <= 0) break; //disconnected
        buf[n] = '\0';
        rstrip(buf);
        if (buf[0] == '\0') continue;

      //command handling
        if (buf[0] == '/') {
            if (strncmp(buf, "/quit", 5) == 0) {
                break;
            } else if (strncmp(buf, "/who", 4) == 0) {
            //lists connected users
                pthread_mutex_lock(&clients_mutex);
                char list[BUFFER_SIZE] = "Online:";
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (clients[i].in_use) {
                        strcat(list, " ");
                        strcat(list, clients[i].name);
                        strcat(list, ",");
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                strcat(list, "\n");
                send_str(sock, list);
            } else if (strncmp(buf, "/nick ", 6) == 0) {
            //change username
                const char *newname = buf + 6;
                if (strlen(newname) == 0) {
                    send_str(sock, "Usage: /nick <newname>\n");
                } else {
                    pthread_mutex_lock(&clients_mutex);
                    if (find_by_name(newname) != -1) {
                        pthread_mutex_unlock(&clients_mutex);
                        send_str(sock, "Name in use. Pick another.\n");
                    } else {
                        for (int i = 0; i < MAX_CLIENTS; ++i) {
                            if (clients[i].in_use && clients[i].sock == sock) {
                                strncpy(clients[i].name, newname, sizeof(clients[i].name)-1);
                                break;
                            }
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        char msg[128];
                        snprintf(msg, sizeof msg, "[system] %s is now known as %s.", name, newname);
                        strncpy(name, newname, sizeof name - 1);
                        system_broadcast("%s", msg);
                    }
                }
            } else if (strncmp(buf, "/msg ", 5) == 0) {
            //Private message
                const char *p = buf + 5;
                char target[32];
                int consumed = 0;
                if (sscanf(p, "%31s %n", target, &consumed) == 1) {
                    const char *text = p + consumed;
                    int idx = find_by_name(target);
                    if (idx == -1) {
                        send_str(sock, "User not found.\n");
                    } else {
                        char pm[BUFFER_SIZE];
                        snprintf(pm, sizeof pm, "[pm %s->%s] %s\n", name, target, text);
                        send_str(clients[idx].sock, pm);
                        send_str(sock, pm);
                    }
                } else {
                    send_str(sock, "Usage: /msg <user> <text>\n");
                }
            } else {
                send_str(sock, "Unknown command.\n");
            }
            continue;
        }
      
//send regular message (to whole chat)
        char ts[16];
        ts_now(ts, sizeof ts);
        char line[BUFFER_SIZE + 64];
        snprintf(line, sizeof line, "[%s] %s: %s\n", ts, name, buf); //prints timestamp and name
        broadcast(line, -1);
        log_line(line);
    }

//cleanup when client disconnects
    system_broadcast("[system] %s left the chat.", name);
    char name_copy[32] = {0};
    remove_client(sock, name_copy);
    return NULL;
}

//Main server entry point

int main(void) {
//initializes client slots
    for (int i = 0; i < MAX_CLIENTS; ++i) clients[i].in_use = 0;

//creates listening socket. Creates TCP socket bound to PORT, listens for new connections
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); //reuse port quickly ######

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; //accepts any IP
    addr.sin_port = htons(PORT); //listening port

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_sock); return 1;
    }
    if (listen(server_sock, 8) < 0) {
        perror("listen"); close(server_sock); return 1;
    }
    printf("chat server listening on port %d...\n", PORT);

//Accept loop
    while (1) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int client_sock = accept(server_sock, (struct sockaddr*)&cliaddr, &clilen);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            perror("accept"); break;
        }

        int idx = add_client(client_sock, &cliaddr);
        if (idx == -1) {
            const char *full = "Server is full. Try again later.\n";
            send_str(client_sock, full);
            close(client_sock);
            continue;
        }
      //creates new thread for the client
        int *arg = malloc(sizeof(int));
        *arg = client_sock;
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, arg) != 0) {
            perror("pthread_create");
            close(client_sock);
        } else {
            pthread_detach(tid); //frees resources when thread exits
        }
    }

    close(server_sock); //close socket
    return 0;
}
