/* ============================================================================
 * FINAL TCP CHAT CLIENT WITH DISCOVERY SERVER
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"

#define DISCOVERY_PORT 9090
#define CHAT_PORT 8080

#define BUFFER_SIZE 1024

int client_socket;

/* ===========================
   RECEIVE THREAD
   =========================== */

void* receive_handler(void* arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        int bytes_received =
            recv(client_socket,
                 buffer,
                 BUFFER_SIZE - 1,
                 0);

        if (bytes_received <= 0)
        {
            printf("\nDisconnected from chat server\n");
            exit(0);
        }

        buffer[bytes_received] = '\0';

        printf("\n%s\n", buffer);

        fflush(stdout);
    }

    return NULL;
}

/* ===========================
   SEND THREAD
   =========================== */

void* send_handler(void* arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        fgets(buffer, BUFFER_SIZE, stdin);

        buffer[strcspn(buffer, "\n")] = '\0';

        send(client_socket,
             buffer,
             strlen(buffer),
             0);

        if (strcmp(buffer, "exit") == 0)
        {
            printf("Exiting...\n");

            close(client_socket);

            exit(0);
        }
    }

    return NULL;
}

/* ===========================
   MAIN
   =========================== */

int main()
{
    int discovery_socket;

    struct sockaddr_in discovery_addr;
    struct sockaddr_in chat_addr;

    char username[50];
    char password[50];
    char buffer[BUFFER_SIZE];

    pthread_t recv_thread;
    pthread_t send_thread;

    /* =====================================================
       STEP 1 : CONNECT TO DISCOVERY SERVER
       ===================================================== */

    discovery_socket = socket(AF_INET,
                              SOCK_STREAM,
                              0);

    if (discovery_socket < 0)
    {
        perror("Discovery socket failed");
        exit(EXIT_FAILURE);
    }

    discovery_addr.sin_family = AF_INET;
    discovery_addr.sin_port = htons(DISCOVERY_PORT);

    inet_pton(AF_INET,
              SERVER_IP,
              &discovery_addr.sin_addr);

    if (connect(discovery_socket,
                (struct sockaddr*)&discovery_addr,
                sizeof(discovery_addr)) < 0)
    {
        perror("Discovery server connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to Discovery Server\n");

    /* =====================================================
       STEP 2 : LOGIN / REGISTER
       ===================================================== */

    int choice;

    printf("\n1. Register\n");
    printf("2. Login\n");

    printf("Enter choice: ");
    scanf("%d", &choice);

    getchar();

    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);

    username[strcspn(username, "\n")] = '\0';

    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);

    password[strcspn(password, "\n")] = '\0';

    /* REGISTER */

    if (choice == 1)
    {
        sprintf(buffer,
                "REGISTER %s %s 127.0.0.1 %d",
                username,
                password,
                CHAT_PORT);
    }

    /* LOGIN */

    else
    {
        sprintf(buffer,
                "LOGIN %s %s",
                username,
                password);
    }

    send(discovery_socket,
         buffer,
         strlen(buffer),
         0);

    /* =====================================================
       STEP 3 : RECEIVE AUTH RESPONSE
       ===================================================== */

    int bytes =
        recv(discovery_socket,
             buffer,
             BUFFER_SIZE - 1,
             0);

    buffer[bytes] = '\0';

    printf("Discovery Server: %s\n", buffer);

    /* =====================================================
       STEP 4 : AUTH CHECK
       ===================================================== */

    if (strcmp(buffer, "REGISTER_SUCCESS") != 0 &&
        strcmp(buffer, "LOGIN_SUCCESS") != 0)
    {
        printf("Authentication failed\n");

        close(discovery_socket);

        return 0;
    }

    close(discovery_socket);

    /* =====================================================
       STEP 5 : CONNECT TO CHAT SERVER
       ===================================================== */

    client_socket = socket(AF_INET,
                           SOCK_STREAM,
                           0);

    if (client_socket < 0)
    {
        perror("Chat socket failed");
        exit(EXIT_FAILURE);
    }

    chat_addr.sin_family = AF_INET;
    chat_addr.sin_port = htons(CHAT_PORT);

    inet_pton(AF_INET,
              SERVER_IP,
              &chat_addr.sin_addr);

    if (connect(client_socket,
                (struct sockaddr*)&chat_addr,
                sizeof(chat_addr)) < 0)
    {
        perror("Chat server connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to Chat Server\n");

    /* =====================================================
       STEP 6 : SEND USERNAME TO CHAT SERVER
       ===================================================== */

    send(client_socket,
         username,
         strlen(username),
         0);

    /* =====================================================
       STEP 7 : COMMANDS
       ===================================================== */

    printf("\n===== CHAT COMMANDS =====\n");

    printf("ALL <msg>\n");
    printf("MSG <user> <msg>\n");
    printf("USERS\n");
    printf("exit\n\n");

    /* =====================================================
       STEP 8 : THREADS
       ===================================================== */

    pthread_create(&recv_thread,
                   NULL,
                   receive_handler,
                   NULL);

    pthread_create(&send_thread,
                   NULL,
                   send_handler,
                   NULL);

    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    close(client_socket);

    return 0;
}