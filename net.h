/*-------------------------------------------------------------------------
 * net.h
 *		Networking support
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#ifndef _YP_NET_H
#define _YP_NET_H_

#include "ypsql.h"

bool net_init(bool dhcp, bool verbose);
bool net_connect(YPconn *conn);
void net_close(YPconn *conn);
void net_poll(YPconn *conn, bool expect);

void net_compact(YPconn *conn);

void net_send(YPconn *conn, char *msg, uint16_t msg_len);
bool net_read_byte(YPconn *conn, char *byte);
void net_skip_n_bytes(YPconn *conn, uint16_t n);
#define net_skip_byte(x) net_skip_n_bytes((x), 1);
YPstatus net_read_int16(YPconn *conn, int16_t *ret);
YPstatus net_read_int32(YPconn *conn, int32_t *ret);
void net_skip_int(YPconn *conn, uint16_t len);
YPstatus net_read_string(YPconn *conn, char **buf, uint16_t buflen);
void net_skip_string(YPconn *conn);

int net_available(YPconn *conn);

uint32_t hton32(uint32_t i);

#endif /* _YP_NET_H_ */
