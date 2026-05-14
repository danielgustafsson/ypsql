/*-------------------------------------------------------------------------
 * ypsql.h
 *		Main YellowedPlasticSQL include file
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#ifndef _YPSQL_H_
#define _YPSQL_H_

#include <cc65.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * Application Metadata
 */
#define YPSQL_NAME "ypsql"
#define YPSQL_VERSION "0.1a"

/*
 * PETSCII Control Characters
 */
#define PETSCII_UP_ARROW 0x91
#define PETSCII_DOWN_ARROW 0x99

/*
 * Interface
 */
#define PROMPT_QUERY "#"

/*
 * Assert checking
 *
 * To save memory we avoid handling cannot-happen errors in production builds
 * and only test them in debug builds with assertions.  Any error which can
 * realistically happen should always be caught and handled.
 */
#ifdef __ASSERT_CHECKING
#define Assert(x)	assert(x)
#else
#define Assert(x)	((void)true)
#endif

/*
 * Misc
 */
#define Max(x, y)		((x) > (y) ? (x) : (y))
#define Min(x, y)		((x) < (y) ? (x) : (y))

typedef enum ConnectionState
{
	CONNECTION_NEEDED = 0,

	INVALID_CONFIG,
	CONNECTION_FAILED,
	CONNECTION_CLOSED,

	/*
	 * This must be the first state at which we have an open TCP connection
	 * to the server, error states where the network is either torn down or
	 * never set up must preceed this.
	 */
	CONNECTION_STARTED,
	CONNECTION_AWAITING_RESPONSE,

	/*
	 * Once authentication has passed, the connection is set up and we can
	 * start issuing queries.
	 */
	AUTHENTICATION_COMPLETE,
	CONNECTION_COMPLETE,

	QUERY_AWAITING_RESPONSE,
	QUERY_AWAITING_DATA,
	QUERY_AWAITING_READY_FOR_QUERY,

	IDLE
};

/*
 * Connection Related stuff
 */
typedef struct YPconn
{
	char	   *username;
	char	   *password;

	/* IP and port to the PostgreSQL server */
	char	   *pg_addr;
	uint16_t	pg_port;

	/* Current state for connection state machine */
	uint16_t	state;

	/* Pointers into the read buffer */
	uint16_t	msg_cur;		/* The next byte to read */
	uint16_t	msg_end;		/* The final byte we know about */

	char	   *sql_state;
} YPconn;

/*
 * Errorhandling
 */
typedef enum YPstatus
{
	YP_OK = 0,

	YP_ERROR_OOM = 1,		/* Failure to allocate memory */
	YP_ERROR_EOF,			/* No data available */
} YPstatus;

/*
 * main.c
 */
void quit(YPconn *conn);
void run_watch(YPconn *conn);

/*
 * util.c
 */
void _a(char *a);
void _p(char *p);

#endif /* _YPSQL_H_ */
