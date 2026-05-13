/* ============================================================================
 * FINAL MULTI-CLIENT TCP CHAT SERVER (THREAD BASED)
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

/* ===========================
   GLOBAL STORAGE
   =========================== */

int client_sockets[MAX_CLIENTS];
char client_names[MAX_CLIENTS][50];
int client_count = 0;

pthread_mutex_t lock;

/* ===========================
   BROADCAST TO ALL CLIENTS
   =========================== */

void broadcast(char *message, int sender_socket)
{
    pthread_mutex_lock(&lock);

    for (int i = 0; i < client_count; i++)
    {
        if (client_sockets[i] != sender_socket)
        {
            send(client_sockets[i],
                 message,
                 strlen(message),
                 0);
        }
    }

    pthread_mutex_unlock(&lock);
}

/* ===========================
   FIND SOCKET BY USERNAME
   =========================== */

int find_socket(char *name)
{
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(client_names[i], name) == 0)
        {
            return client_sockets[i];
        }
    }

    return -1;
}

/* ===========================
   CHECK DUPLICATE USERNAME
   =========================== */

int username_exists(char *name)
{
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(client_names[i], name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/* ===========================
   REMOVE CLIENT
   =========================== */

void remove_client(int socket)
{
    pthread_mutex_lock(&lock);

    for (int i = 0; i < client_count; i++)
    {
        if (client_sockets[i] == socket)
        {
            for (int j = i; j < client_count - 1; j++)
            {
                client_sockets[j] = client_sockets[j + 1];
                strcpy(client_names[j], client_names[j + 1]);
            }

            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&lock);
}

/* ===========================
   SEND ONLINE USER LIST
   =========================== */

void send_user_list(int client_socket)
{
    char list[BUFFER_SIZE] = "\nOnline Users:\n";

    pthread_mutex_lock(&lock);

    for (int i = 0; i < client_count; i++)
    {
        strcat(list, "- ");
        strcat(list, client_names[i]);
        strcat(list, "\n");
    }

    pthread_mutex_unlock(&lock);

    send(client_socket,
         list,
         strlen(list),
         0);
}

/* ===========================
   CLIENT THREAD
   =========================== */

void* handle_client(void* arg)
{
    int client_socket = (int)(long)arg;

    char username[50];
    char buffer[BUFFER_SIZE];

    /* ===========================
       RECEIVE USERNAME
       =========================== */

    int n = recv(client_socket,
                 username,
                 sizeof(username) - 1,
                 0);

    if (n <= 0)
    {
        close(client_socket);
        return NULL;
    }

    username[n] = '\0';

    /* Remove newline if exists */
    username[strcspn(username, "\n")] = '\0';

    /* ===========================
       CHECK DUPLICATE USERNAME
       =========================== */

    pthread_mutex_lock(&lock);

    if (username_exists(username))
    {
        send(client_socket,
             "USERNAME_ALREADY_EXISTS",
             23,
             0);

        pthread_mutex_unlock(&lock);

        close(client_socket);
        return NULL;
    }

    strcpy(client_names[client_count], username);
    client_sockets[client_count] = client_socket;
    client_count++;

    pthread_mutex_unlock(&lock);

    printf("%s joined the chat\n", username);

    char join_msg[BUFFER_SIZE];

    sprintf(join_msg,
            "[SERVER]: %s joined the chat",
            username);

    broadcast(join_msg, client_socket);

    /* ===========================
       CHAT LOOP
       =========================== */

    while (1)
    {
        int bytes = recv(client_socket,
                         buffer,
                         BUFFER_SIZE - 1,
                         0);

        if (bytes <= 0)
        {
            printf("%s disconnected\n", username);
            break;
        }

        buffer[bytes] = '\0';

        /* ===========================
           EXIT
           =========================== */

        if (strcmp(buffer, "exit") == 0)
        {
            printf("%s left the chat\n", username);
            break;
        }

        /* ===========================
           BROADCAST
           ALL hello everyone
           =========================== */

        else if (strncmp(buffer, "ALL ", 4) == 0)
        {
            char msg[BUFFER_SIZE];

            sprintf(msg,
                    "[%s]: %s",
                    username,
                    buffer + 4);

            printf("%s\n", msg);

            broadcast(msg, client_socket);
        }

        /* ===========================
           PRIVATE MESSAGE
           MSG user hello
           =========================== */

        else if (strncmp(buffer, "MSG ", 4) == 0)
        {
            char target[50];
            char private_msg[BUFFER_SIZE];

            sscanf(buffer + 4,
                   "%s %[^\n]",
                   target,
                   private_msg);

            int target_socket = find_socket(target);

            if (target_socket != -1)
            {
                char final_msg[BUFFER_SIZE];

                sprintf(final_msg,
                        "[PM from %s]: %s",
                        username,
                        private_msg);

                send(target_socket,
                     final_msg,
                     strlen(final_msg),
                     0);
            }
            else
            {
                send(client_socket,
                     "User not found",
                     14,
                     0);
            }
        }

        /* ===========================
           USERS LIST
           =========================== */

        else if (strcmp(buffer, "USERS") == 0)
        {
            send_user_list(client_socket);
        }

        /* ===========================
           INVALID COMMAND
           =========================== */

        else
        {
            send(client_socket,
                 "Invalid command",
                 15,
                 0);
        }
    }

    /* ===========================
       CLEANUP
       =========================== */

    remove_client(client_socket);

    close(client_socket);

    char leave_msg[BUFFER_SIZE];

    sprintf(leave_msg,
            "[SERVER]: %s left the chat",
            username);

    broadcast(leave_msg, client_socket);

    return NULL;
}

/* ===========================
   MAIN SERVER
   =========================== */

int main()
{
    int server_socket;
    int client_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_len = sizeof(client_address);

    pthread_mutex_init(&lock, NULL);

    /* ===========================
       CREATE SOCKET
       =========================== */

    server_socket = socket(AF_INET,
                           SOCK_STREAM,
                           0);

    if (server_socket < 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    /* ===========================
       REUSE ADDRESS
       =========================== */

    int opt = 1;

    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    /* ===========================
       SERVER ADDRESS
       =========================== */

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    /* ===========================
       BIND
       =========================== */

    if (bind(server_socket,
             (struct sockaddr*)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    /* ===========================
       LISTEN
       =========================== */

    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chat Server running on port %d\n", PORT);

    /* ===========================
       ACCEPT LOOP
       =========================== */

    while (1)
    {
        client_socket = accept(server_socket,
                               (struct sockaddr*)&client_address,
                               &client_len);

        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("New connection: %s:%d\n",
               inet_ntoa(client_address.sin_addr),
               ntohs(client_address.sin_port));

        pthread_t tid;

        pthread_create(&tid,
                       NULL,
                       handle_client,
                       (void*)(long)client_socket);

        pthread_detach(tid);
    }

    close(server_socket);

    return 0;
}