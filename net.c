/*-------------------------------------------------------------------------
 * net.c
 *		Network support code for setting up network and reading/writing
 *		from a socket.  This file only contains low-level functionality
 *		for network access, higher level functionality should be placed
 *		in the relevant pg_*.c file.
 *
 * net.c is responsible for maintaining two buffers, one for received data yet
 * to be consumed, and one for data which has yet to be sent to the peer.  To
 * save memory we try to avoid copying and double-buffering as far as possible.
 *
 * Receiving data
 * --------------
 * The reader keeps a pointer to the current byte to read in the YPconn struct,
 * which is an offset into r_buf. Any reading will start from there and move
 * the offset ahead by the amount read.  YPconn also contain an msg_end pointer
 * indicating the number of bytes the upper layer knows about. The buffer may
 * contain more bytes read from the wire, which have yet to be acknowledged by
 * upper layers. The total number of bytes read is tracked by r_buf_len which
 * is private to net.c. Once data has been consumed, compaction can be used to
 * copy the region [conn->cur,r_buf_len] to offset zero and thus freeing up
 * bytes at the end of the buffer for more data to be read into.
 *
 * +----------------------------------------------------+
 * |  |  r_buf               |              |           |
 * +----------------------------------------------------+
 *    ^                      ^              ^
 *  conn->msg_cur       conn->msg_end     r_buf_len

 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#include <cc65.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ypsql.h"
#include "net.h"

#include "ip65/inc/ip65.h"

/* Buffer for reading */
unsigned char r_buf[3072];
uint16_t r_buf_len;

#define REMAINING_BYTES (sizeof(r_buf) - r_buf_len)
#define COMPACT_THRESHOLD 512

static void tcp_recv(const unsigned char *tcp_buffer, int len);

uint32_t hton32(uint32_t i)
{
	return ((i << 24) & 0xff000000) | ((i << 8) & 0x00ff0000) |
		   ((i >> 8) & 0x0000ff00) | ((i >> 24) & 0x000000ff);
}

/*
 * net_init
 *
 * Returns true on success and false on failure.  A specific address to bind
 * to can be set by initializing cfg_ip/cfg_netmarks and set dhcp to false,
 * else dhcp will be used.  If verbose is true, additional debugging info like
 * the result of the DHCP lease is printed to the screen.
 */
bool
net_init(bool dhcp, bool verbose)
{
	uint8_t init = ETH_INIT_DEFAULT;
	bool res;

	r_buf_len = 0;
	memset(r_buf, 0, sizeof(r_buf));

	res = ip65_init(init);
	if (res)
	{
		printf("- ERROR: %s\n", ip65_strerror(ip65_error));
		goto error;
	}

	if (verbose)
		printf("* Interface: %s\n", eth_name);

	/* No address was specified, initialize DHCP */
	if (dhcp)
	{
		printf("* DHCP: acquiring lease..");
		res = dhcp_init();
		printf("\n");
		if (res)
		{
			printf("- ERROR: %s\n", ip65_strerror(ip65_error));
			goto error;
		}
		if (verbose)
		{
			printf("* IP: %s", dotted_quad(cfg_ip));
			printf("/%s\n", dotted_quad(cfg_netmask));
			printf("* Gateway: %s\n", dotted_quad(cfg_gateway));
		}
	}

	return true;

error:
	return false;
}

/*
 * net_connect
 *
 * Establish a TCP connection to the remote server defined in YPconn.  Returns
 * true on successfull connection.
 */
bool
net_connect(YPconn *conn)
{
	uint32_t	server;

	printf("* Connecting: %s:%i\n", conn->pg_addr, conn->pg_port);
	server = parse_dotted_quad(conn->pg_addr);
	if (!server)
	{
		conn->state = INVALID_CONFIG;
		printf("- ERROR: unable to parse server address \"%s\"", conn->pg_addr);
		return false;
	}

	if (tcp_connect(server, conn->pg_port, tcp_recv))
	{
		printf("- ERROR: %s\n", ip65_strerror(ip65_error));
		conn->state = CONNECTION_FAILED;
		return false;
	}

	conn->state = CONNECTION_STARTED;
	return true;
}

void net_close(YPconn *conn)
{
	(void) conn;
	tcp_close();
}

/*
 * net_poll
 *
 * Ask the TCP stack to read a packet from the network. If conn is NULL then
 * it is assumed to be early enough in the connection cycle that no connection
 * exist yet.
 */
void
net_poll(YPconn *conn, bool expect)
{
	int r_start = r_buf_len;
	int i;

	/* If we risk running out of space, try to clear some room first */
	if (conn && (REMAINING_BYTES < COMPACT_THRESHOLD))
		net_compact(conn);
	for (i = 0; i < 3; i++)
	{
		ip65_process();
		if (r_start == r_buf_len && expect)
			sleep(1);
		else
			break;
	}
	if (!conn)
		return;

	/* Make sure the application knows to see all the data */
	conn->msg_end = r_buf_len;
}

/*
 * net_compact
 *
 * As bytes are consumed from the input buffer, shift the buffer contents to
 * the start to make room for more data read from the wire at the end of the
 * buffer;
 */
void
net_compact(YPconn *conn)
{
	printf("* net_compact: %i\n", r_buf_len);
	if (conn->msg_cur == 0)
		return;

	memmove(r_buf, r_buf + conn->msg_cur, sizeof(r_buf) - conn->msg_cur);
	r_buf_len -= conn->msg_cur;
	conn->msg_end -= conn->msg_cur;
	conn->msg_cur = 0;
}

/*
 * net_send
 *
 * Transmit the contents of msg. TODO: better errorhandling to return error
 * back to caller.
 */
void
net_send(YPconn *conn, char *msg, uint16_t msg_len)
{
	uint16_t len = msg_len;
	uint16_t offset = 0;
	(void) conn;

	while (len > 0)
	{
		uint16_t send_len = Min(len, 1460);
		if (tcp_send((const unsigned char *)(msg + offset), len))
		{
			printf("- ERROR: %s\n", ip65_strerror(ip65_error));
			break;
		}
		else
		{
			offset += send_len;
			len -= send_len;
		}
	}
}

/*
 * tcp_recv
 *
 * Callback for retrieving data on the socket, copying it to the local buffer.
 */
static void
tcp_recv(const unsigned char *tcp_buffer, int len)
{
	/* Connection has been closed, clear the buffer and exit */
	if (len == -1)
	{
		memset(r_buf, '\0', sizeof(r_buf));
		return;
	}

	if (len > REMAINING_BYTES)
	{
		printf("* ERROR: buffer overflow, need %i have %i\n",
			   len, REMAINING_BYTES);
		quit(NULL);
	}

	/* There is space in the buffer, copy the data in r_buf */
	memcpy(r_buf + r_buf_len, tcp_buffer, len);
	r_buf_len += len;
}

/*
 * net_read_byte
 *
 * Returns the next byte from the input buffer.  This function has no failure
 * condition but it follows the common API format regardless for consistency.
 */
bool
net_read_byte(YPconn *conn, char *byte)
{
	if (conn->msg_cur + 1 > r_buf_len)
	{
		net_poll(conn, true);
		if (conn->msg_cur + 1 > r_buf_len)
			return false;
	}

	*byte = r_buf[conn->msg_cur++];
	return true;
}

void
net_skip_n_bytes(YPconn *conn, uint16_t n)
{
	if (conn->msg_cur + n > r_buf_len)
		net_poll(conn, true);
	conn->msg_cur += n;
}

/*
 * net_read_int16
 *
 * Read a 2 byte integer from the message buffer and return in local byte
 * order.  Returns YP_EOF if not enough data exist in the socket buffer.
 */
YPstatus
net_read_int16(YPconn *conn, int16_t *ret)
{
	int16_t	two;

	/* Make sure there are enough bytes in the buffer to satisfy the request */
	if (conn->msg_cur + 2 > r_buf_len)
	{
		net_poll(conn, true);
		if (conn->msg_cur + 2 > r_buf_len)
			return YP_ERROR_EOF;
	}

	memcpy(&two, r_buf + conn->msg_cur, 2);
	conn->msg_cur += 2;
	*ret = ntohs(two);

	return YP_OK;
}

/*
 * net_read_int32
 *
 * Read a 4 byte integer from the message buffer and return in local byte
 * order.  Returns YP_EOF if not enough data exist in the socket buffer.
 */
YPstatus
net_read_int32(YPconn *conn, int32_t *ret)
{
	int32_t	four;

	/* Make sure there are enough bytes in the buffer to satisfy the request */
	if (conn->msg_cur + 4 > r_buf_len)
	{
		net_poll(conn, true);
		if (conn->msg_cur + 4 > r_buf_len)
			return YP_ERROR_EOF;
	}

	memcpy(&four, r_buf + conn->msg_cur, 4);
	conn->msg_cur += 4;
	*ret = ntohl(four);

	return YP_OK;
}


void
net_skip_int(YPconn *conn, uint16_t len)
{
	if (conn->msg_cur + len > r_buf_len)
		net_poll(conn, true);
	conn->msg_cur += len;
}

YPstatus
net_read_string(YPconn *conn, char **buf, uint16_t buflen)
{
	uint16_t len = 0;

	while ((conn->msg_cur + len <= r_buf_len) && r_buf[conn->msg_cur + len])
	{
		len++;
		if (conn->msg_cur + len > r_buf_len)
			net_poll(conn, false);
	}

	conn->msg_cur += len;
	if (*buf)
	{
		memset(*buf, '\0', buflen);
		memcpy(*buf, r_buf + conn->msg_cur, Min(buflen, len) - 1);
	}

	return YP_OK;
}

void
net_skip_string(YPconn *conn)
{
	net_read_string(conn, NULL, 0);
}

int
net_available(YPconn *conn)
{
	return r_buf_len - conn->msg_cur;
}
