/*-------------------------------------------------------------------------
 * main.c
 *		Main entrypoint for YellowedPlasticSQL
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <conio.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "connect.h"
#include "net.h"
#include "pg_protocol.h"
#include "query.h"
#include "ypsql.h"

static char query_buffer[128];

/*
 * prompt
 *
 * Displays a new query prompt and reads a query into the passed buffer of
 * size len.  A query is considered to be sent when the user press ENTER or
 * if the query exceeds len.
 */
int
prompt(char *buf, int len)
{
	char c = 0;
	char *bufp = buf;

	memset(buf, '\0', len);

	printf("%s ", PROMPT_QUERY);

	/* Set the cursor to blinking to indicate waiting for keystroke */
	cursor(1);

	while(1)
	{
		/* Read a character from the keyboard, blocking */
		c = cgetc();

		/* We consider a command to be done when the user hits ENTER */
		if (c == '\n')
			return bufp - buf;

		/*
		 * For now let's block up and down arrow movement, it's a TODO to make
		 * the query editing a more C64ish
		 */
		if (c == PETSCII_UP_ARROW || c == PETSCII_DOWN_ARROW)
			continue;

		printf("%c", c);
		*bufp = c;
		bufp++;

		if (bufp - buf >= len)
			return len;
	}

	/* Unreachable */
}

/*
 * banner
 *
 * Display the login banner, optionally after clearing the screen, and return.
 */
void
banner(int clear)
{
	if (clear)
		clrscr();
	printf("%s %s, PostgreSQL client\n\n", YPSQL_NAME, YPSQL_VERSION);
}

/*
 * quit
 *
 * Disconnect from server and release any open resources before exiting the
 * application.
 */
void
quit(YPconn *conn)
{
	if (conn != NULL)
	{
		/* If we have an open TCP connection, close before exiting */
		if (conn->state >= CONNECTION_STARTED)
			net_close(conn);
	}
	exit(0);
}

int
main(void)
{
	char cmd[128];
	short cmd_len = 0;
	bool res;
	YPconn conn;

	/*
	 * Initialize globals
	 */
	memset(query_buffer, '\0', sizeof(query_buffer));

	/* Initialize network */
	printf("* Initializing network..\n");
	res = net_init(true, false);
	if (!res)
	{
		printf("- ERROR: Unable to set up network\n");
		quit(NULL);
	}

	/* TODO: Read these from keyboard input */
	conn.username = "postgres";
	conn.password = "postgres";
	conn.pg_addr = "192.168.1.1";
	conn.pg_port = 5432;
	conn.msg_cur = 0;
	conn.msg_end = 0;

	connect_db(&conn);

	/*
	 * Main loop around command prompt. Read input from the user and dispatch
	 * to the correct handler.
	 */
	while(1)
	{
		cmd_len = prompt(cmd, sizeof(cmd));
		if (!cmd_len)
		{
			printf("\n");
			continue;
		}

		/*
		 * Meta commands start with ':'
		 */
		if (cmd[0] == ':')
			command_run(&conn, cmd, cmd_len);
		/*
		 * SQL query to send to Postgres
		 */
		else
		{
			printf("\n");
			/* Save the current querytext */
			memset(query_buffer, '\0', sizeof(query_buffer));
			memcpy(query_buffer, cmd, cmd_len);

			send_query(&conn, query_buffer);
		}
	}

	/* Unreachable */
}

void
run_watch(YPconn *conn)
{
	if (!query_buffer[0] || strlen(query_buffer) == 0)
	{
		printf("* ERROR: no query in buffer\n");
		return;
	}

	while (1)
	{
		send_query(conn, query_buffer);
		sleep(1);
		if (kbhit() != 0)
		{
			printf("* Interrupted by user\n");
			return;
		}
	}
}
