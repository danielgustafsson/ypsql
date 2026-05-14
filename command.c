/*-------------------------------------------------------------------------
 * command.c
 *		Functionality for running commands in YellowedPlasticSQL
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#include <stdio.h>
#include <conio.h>
#include <string.h>

#include "ypsql.h"
#include "command.h"

void
print_help(void)
{
	printf("\n%s %s, PostgreSQL client\n\n", YPSQL_NAME, YPSQL_VERSION);
	printf("Meta commands:\n");
	printf(" :clear   - Clear screen\n");
	printf(" :help    - Print helpscreen\n");
	printf(" :host    - Print server address\n");
	printf(" :quit    - Logout and quit\n");
	printf(" :watch   - Run query every 1s\n");
}

static void
print_host(YPconn *conn)
{
	printf("\nHost: %s\n", conn->pg_addr);
	printf("Port: %i\n", conn->pg_port);
}

void
command_run(YPconn *conn, char *cmd, int len)
{
	if (len < 2)
		return;

	switch (cmd[1])
	{
		/* C - :clear */
		case 'c':
			if (strcmp(cmd, ":clear") == 0)
				clrscr();
			break;

		/* H - :help, :host */
		case 'h':
			if (strcmp(cmd, ":help") == 0)
				print_help();
			else if (strcmp(cmd, ":host") == 0)
				print_host(conn);
			break;

		/* Q - :quit */
		case 'q':
			if (strcmp(cmd, ":quit") == 0)
				quit(conn);

		/* W - :watch */
		case 'w':
			if (strcmp(cmd, ":watch") == 0)
				run_watch(conn);
			break;

		default:
			printf("ERROR: unknown command\n\n");
			break;
	}
}
