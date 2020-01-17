#ifndef __WORK_H__
#define __WORK_H__

#include "client.h"

int cli_infp_send_login(sock_t* sock, cli_infp_t* infp);
int cli_infp_send_heart(sock_t* sock, cli_infp_t* infp);
int cli_infp_recv_do(sock_t *sock, struct sockaddr_in *addr);

#endif

