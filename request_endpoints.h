#ifndef REQUEST_ENDPOINT_H
#define REQUEST_ENDPOINT_H

#include "http_message.h"

void handle_static_endpoint(int client_socket, http_client_message_t *msg);
void handle_stats_endpoint(int client_socket);
void handle_calc_endpoint(int client_socket, http_client_message_t *msg);

#endif // REQUEST_ENDPOINT_H
