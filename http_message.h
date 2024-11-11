#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

typedef struct msg {
  char *method;
  char *path;
  char *http_version;
  char *body;
  int body_length;
  char *headers;
} http_client_message_t;

typedef enum { BAD_REQUEST, CLOSED_CONNECTION, MESSAGE } http_read_result_t;

void read_http_client_message(int client_socket, http_client_message_t **msg,
                              http_read_result_t *result, int *recieved_bytes);

void http_client_message_free(http_client_message_t *msg);

#endif // HTTP_MESSAGE_H