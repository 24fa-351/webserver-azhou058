#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

int request_count = 0;
long total_bytes_received = 0;
long total_bytes_sent = 0;
pthread_mutex_t stats_mutex;

void *handle_client(void *client_socket);
void handle_static(int sock, const char *path);
void handle_stats(int sock);
void handle_calc(int sock, const char *query);
const char *get_mime_type(const char *path);

int main(int argc, char *argv[]) {
    int port = 8080;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    pthread_mutex_init(&stats_mutex, NULL);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d\n", port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t thread;
        int *client_sock = malloc(sizeof(int));
        *client_sock = client_socket;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);
    }

    close(server_socket);
    pthread_mutex_destroy(&stats_mutex);
    return 0;
}

void *handle_client(void *client_socket) {
    int sock = *(int*)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        perror("Receive failed");
        close(sock);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    pthread_mutex_lock(&stats_mutex);
    total_bytes_received += bytes_received;
    request_count++;
    pthread_mutex_unlock(&stats_mutex);

    char method[8], path[256], version[16];
    sscanf(buffer, "%7s %255s %15s", method, path, version);

    if (strcmp(method, "GET") == 0) {
        if (strncmp(path, "/static/", 8) == 0) {
            handle_static(sock, path);
        } else if (strcmp(path, "/stats") == 0) {
            handle_stats(sock);
        } else if (strncmp(path, "/calc", 5) == 0) {
            char *query = strchr(path, '?');
            if (query != NULL) {
                handle_calc(sock, query + 1);
            } else {
                send(sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0);
            }
        } else {
            send(sock, "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found", 61, 0);
        }
    } else {
        send(sock, "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\n400 Bad Request", 67, 0);
    }

    close(sock);
    return NULL;
}

void handle_static(int sock, const char *path) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), ".%s", path);

    FILE *file = fopen(full_path, "rb");
    if (!file) {
        send(sock, "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found", 61, 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    const char *mime_type = get_mime_type(full_path);
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", mime_type, file_size);
    send(sock, header, strlen(header), 0);

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        send(sock, file_buffer, bytes_read, 0);
        pthread_mutex_lock(&stats_mutex);
        total_bytes_sent += bytes_read;
        pthread_mutex_unlock(&stats_mutex);
    }

    fclose(file);
}

void handle_stats(int sock) {
    pthread_mutex_lock(&stats_mutex);
    char stats_html[256];
    snprintf(stats_html, sizeof(stats_html),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
             "<html><body><h1>Server Stats</h1>"
             "<p>Requests: %d</p><p>Bytes Received: %ld</p>"
             "<p>Bytes Sent: %ld</p></body></html>",
             request_count, total_bytes_received, total_bytes_sent);
    pthread_mutex_unlock(&stats_mutex);

    send(sock, stats_html, strlen(stats_html), 0);
    pthread_mutex_lock(&stats_mutex);
    total_bytes_sent += strlen(stats_html);
    pthread_mutex_unlock(&stats_mutex);
}

void handle_calc(int sock, const char *query) {
    int a = 0, b = 0;
    if (sscanf(query, "a=%d&b=%d", &a, &b) == 2) {
        int sum = a + b;
        char calc_result[128];
        snprintf(calc_result, sizeof(calc_result),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                 "<html><body><h1>Calc Result</h1><p>%d + %d = %d</p></body></html>",
                 a, b, sum);

        send(sock, calc_result, strlen(calc_result), 0);
        pthread_mutex_lock(&stats_mutex);
        total_bytes_sent += strlen(calc_result);
        pthread_mutex_unlock(&stats_mutex);
    } else {
        send(sock, "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\n400 Bad Request", 67, 0);
    }
}

const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0) return "text/html";
        if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
        if (strcmp(ext, ".png") == 0) return "image/png";
        if (strcmp(ext, ".css") == 0) return "text/css";
        if (strcmp(ext, ".js") == 0) return "application/javascript";
    }
    return "application/octet-stream";
}
