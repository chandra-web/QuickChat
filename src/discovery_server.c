/* ============================================================================
 * DISCOVERY SERVER (FIXED VERSION)
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9090
#define BUFFER_SIZE 1024
#define MAX_USERS 100

struct User
{
    char username[50];
    char password[50];
    char ip[50];
    int port;
};

struct User users[MAX_USERS];
int user_count = 0;

int find_user(char *username)
{
    for (int i = 0; i < user_count; i++)
    {
        if (strcmp(users[i].username, username) == 0)
            return i;
    }
    return -1;
}

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        int bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes <= 0)
            break;

        buffer[bytes] = '\0';

        printf("Received: %s\n", buffer);

        /* REGISTER */
        if (strncmp(buffer, "REGISTER", 8) == 0)
        {
            char username[50], password[50], ip[50];
            int port;

            sscanf(buffer, "REGISTER %s %s %s %d",
                   username, password, ip, &port);

            if (find_user(username) != -1)
            {
                send(client_socket,
                     "USER_EXISTS",
                     strlen("USER_EXISTS"),
                     0);
            }
            else
            {
                strcpy(users[user_count].username, username);
                strcpy(users[user_count].password, password);
                strcpy(users[user_count].ip, ip);
                users[user_count].port = port;
                user_count++;

                send(client_socket,
                     "REGISTER_SUCCESS",
                     strlen("REGISTER_SUCCESS"),
                     0);

                printf("Registered: %s\n", username);
            }
        }

        /* LOGIN */
        else if (strncmp(buffer, "LOGIN", 5) == 0)
        {
            char username[50], password[50];

            sscanf(buffer, "LOGIN %s %s", username, password);

            int idx = find_user(username);

            if (idx != -1 &&
                strcmp(users[idx].password, password) == 0)
            {
                send(client_socket,
                     "LOGIN_SUCCESS",
                     strlen("LOGIN_SUCCESS"),
                     0);

                printf("Login success: %s\n", username);
            }
            else
            {
                send(client_socket,
                     "LOGIN_FAILED",
                     strlen("LOGIN_FAILED"),
                     0);
            }
        }

        /* LOOKUP */
        else if (strncmp(buffer, "LOOKUP", 6) == 0)
        {
            char username[50];
            sscanf(buffer, "LOOKUP %s", username);

            int idx = find_user(username);

            if (idx != -1)
            {
                char response[100];
                sprintf(response, "%s %d",
                        users[idx].ip,
                        users[idx].port);

                send(client_socket,
                     response,
                     strlen(response),
                     0);
            }
            else
            {
                send(client_socket,
                     "NOT_FOUND",
                     strlen("NOT_FOUND"),
                     0);
            }
        }
    }

    close(client_socket);
    printf("Client disconnected\n");
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    listen(server_socket, 5);

    printf("Discovery Server running on port %d\n", PORT);

    while (1)
    {
        client_socket = accept(server_socket,
                                (struct sockaddr*)&client_addr,
                                &len);

        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("Client connected\n");

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}