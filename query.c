/*-------------------------------------------------------------------------
 * query.c
 *		Functionality for sending queries to PostgreSQL, and processing any
 *		results.
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#include "ypsql.h"
#include "net.h"
#include "pg_protocol.h"

static void state_machine(YPconn *conn);

YPstatus
send_query(YPconn *conn, char *sql)
{
	char *q = NULL;
	uint16_t q_len = 0;

	if (build_Query(conn, sql, &q, &q_len) == YP_OK)
	{
		net_send(conn, q, q_len);
		free(q);

		conn->state = QUERY_AWAITING_RESPONSE;
		state_machine(conn);
	}
	else
	{
		printf("* ERROR: out of memory\n");
		return YP_ERROR_OOM;
	}

	return YP_OK;
}

static void
state_machine(YPconn *conn)
{
	bool expect = true;
	char pkt_type = 0;

keep_going:
	net_poll(conn, expect);

	switch (conn->state)
	{
		case QUERY_AWAITING_RESPONSE:
			/*
			 * Inspect the identifier byte, or keep polling in case there is
			 * nothing to read.
			 */
			if (!net_read_byte(conn, &pkt_type))
				goto keep_going;

			/*
			 * If we get a row description, print to the user and wait for the
			 * actual query data.
			 */
			if (pkt_type == MSG_ROW_DESCRIPTION)
			{
				read_RowDescription(conn);
				conn->state = QUERY_AWAITING_DATA;
			}
			/* The query errored out, print error to the user */
			else if (pkt_type == MSG_ERROR_RESPONSE)
			{
				read_ErrorResponse(conn);
				conn->state = QUERY_AWAITING_READY_FOR_QUERY;
			}
			else if (pkt_type == MSG_COMMAND_COMPLETE)
			{
				return;
			}
			else if (pkt_type == MSG_COPYIN_RESPONSE)
			{
				return;
			}
			else if (pkt_type == MSG_COPYOUT_RESPONSE)
			{
				return;
			}

			goto keep_going;

		/*
		 * ReadyForQuery is the final message sent during query processing and
		 * will be sent regardless of if the query processed fine or errored
		 * out.
		 */
		case QUERY_AWAITING_READY_FOR_QUERY:

			if (!net_read_byte(conn, &pkt_type))
				goto keep_going;

			if (pkt_type == MSG_READY_FOR_QUERY)
			{
				read_ReadyForQuery(conn);
				conn->state = IDLE;
				return;
			}

		case QUERY_AWAITING_DATA:
			if (!net_read_byte(conn, &pkt_type))
				goto keep_going;

			if (pkt_type == MSG_DATA_ROW)
			{
				read_DataRow(conn);
			}
			if (pkt_type == MSG_COMMAND_COMPLETE)
			{
				read_CommandComplete(conn);
				conn->state = QUERY_AWAITING_READY_FOR_QUERY;
			}

			goto keep_going;

		default:
			break;
	}
}
