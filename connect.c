/*-------------------------------------------------------------------------
 * connect.c
 *		Connection oriented routines
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

#include "ypsql.h"
#include "connect.h"
#include "pg_protocol.h"
#include "net.h"

static bool perform_auth(YPconn *conn, int auth_type);
static void state_machine(YPconn *conn);

void
connect_db(YPconn *conn)
{
	bool ret;

	Assert(conn->state == CONNECTION_NEEDED);

	ret = net_connect(conn);
	if (ret)
	{
		conn->state = CONNECTION_STARTED;
	}
	else
	{
		conn->state = CONNECTION_FAILED;
		printf("* Connection to postgres failed\n");
		quit(conn);
	}

	state_machine(conn);
}

static void
state_machine(YPconn *conn)
{
	char	   *pkt;
	uint16_t	pkt_len;
	char		pkt_type;
	bool		expect = false;

keep_going:
	net_poll(conn, expect);

	switch(conn->state)
	{
		case CONNECTION_NEEDED:
			connect_db(conn);
			break;

		case CONNECTION_STARTED:
			/*
			 * We have a TCP connection to the server, and this is where the
			 * protocol allows for negotiating TLS or GSSAPI encryption, but
			 * since we don't support either we go straight to the startup
			 * message.
			 */
			pkt = build_startup_message(conn, &pkt_len);
			if (!pkt)
				goto error;
			net_send(conn, pkt, pkt_len);
			free(pkt);
			pkt_len = 0;
			conn->state = CONNECTION_AWAITING_RESPONSE;
			expect = true;
			goto keep_going;

		case CONNECTION_AWAITING_RESPONSE:
		{
			int32_t		msg_len;
			int32_t		auth_type;

			/*
			 * Look at the first byte in the waiting message, or keep polling
			 * the network in case there is nothing to read. We just want to
			 * look at it so put it back on the buffer once read.
			 */
			net_read_byte(conn, &pkt_type);

			/*
			 * The only valid messages from the server at this point are auth
			 * requests, error responses or protocol version negotiation.  If
			 * any other message is passed, error out.
			 */
			if (pkt_type != MSG_AUTH_REQUEST &&
				pkt_type != MSG_ERROR_RESPONSE &&
				pkt_type != MSG_NEGOTIATE_PROTOCOL_VERSION)
			{
				printf("* ERROR: Expected auth request, got %c", pkt_type);
				quit(conn);
			}

			/* TODO: message format/length validation */
			net_read_int32(conn, &msg_len);

			/*
			 * ErrorResponse Messages. The connection attempt has been rejected
			 * and the server will immediately close the connection.  Display
			 * the message any useful diagnostocs and then error out as well.
			 */
			if (pkt_type == MSG_ERROR_RESPONSE)
			{
				read_ErrorResponse(conn);
				/*
				 * If the server wants us to try another host then error out
				 * with an informative error message since we don't support
				 * that yet.
				 */
				if (strcmp(conn->sql_state, ERRCODE_CANNOT_CONNECT_NOW) == 0)
				{
					printf("* ERROR: ERRCODE_CANNOT_CONNECT_NOW\n");
					quit(conn);
				}
			}

			/* Not supported as of yet */
			else if (pkt_type == MSG_NEGOTIATE_PROTOCOL_VERSION)
			{
				printf("* ERROR: MSG_NEGOTIATE_PROTOCOL_VERSION\n");
				quit(conn);
			}

			/* At this point we know it is an authentication request */
			Assert(pkt_type == MSG_AUTH_REQUEST);

			net_read_int32(conn, &auth_type);
			if (!perform_auth(conn, auth_type))
			{
				printf("* ERROR: perform_auth OOM\n");
				quit(conn);
			}

			expect = true;
			goto keep_going;
		}

		/*
		 * If we received an auth_request_ok message then we are done with
		 * setting up the authentiction. Next up is reading the connection
		 * setup from the server.
		 */
		case AUTHENTICATION_COMPLETE:
			expect = false;
			net_read_byte(conn, &pkt_type);

			/*
			 * I have yet to figure out why the code need to sleep before
			 * reading the next message, but when not doing it the message
			 * length calculation overflow and things behave badly.  Some
			 * timing related bug is lurking in these backwaters.
			 */
			if (pkt_type == MSG_BACKEND_KEY_DATA)
			{
				printf(".");
				sleep(1);
				read_BackendKeyData(conn);
				goto keep_going;
			}

			if (pkt_type == MSG_PARAMETER_STATUS)
			{
				printf(".");
				sleep(1);
				read_ParameterStatus(conn);
				goto keep_going;
			}

			if (pkt_type == MSG_ERROR_RESPONSE)
			{
				printf(".");
				read_ErrorResponse(conn);
				goto keep_going;
			}

			if (pkt_type == MSG_NOTICE_RESPONSE)
			{
				printf(".");
				read_NoticeResponse(conn);
				goto keep_going;
			}

			if (pkt_type == MSG_READY_FOR_QUERY)
			{
				printf(".");
				read_ReadyForQuery(conn);
				conn->state = CONNECTION_COMPLETE;
				printf("\n* Connected to PostgreSQL\n");
				return;
			}

			printf("* Unknown pkt_type %i\n", pkt_type);

			goto keep_going;

		default:
			/* TODO: Errorhandling */
			break;
	}
error:
	/* TODO: Errorhandling */
	return;
}



static bool
perform_auth(YPconn *conn, int auth_type)
{
	char *packet;
	uint16_t packet_len;

	switch (auth_type)
	{
		case MSG_AUTH_REQUEST_OK:
			printf("* Authentication complete\n* Reading connection setup ");
			conn->state = AUTHENTICATION_COMPLETE;
			return true;
		case MSG_AUTH_REQUEST_MD5:
			printf("* MD5 auth\n");
			quit(conn);
		case MSG_AUTH_REQUEST_PASSWORD:
			if (build_PasswordMessage(conn, &packet, &packet_len) == YP_OK)
			{
				net_send(conn, packet, packet_len);
				free(packet);
				return true;
			}
			break;

		case MSG_AUTH_REQUEST_KRB4:
		case MSG_AUTH_REQUEST_KRB5:
		case MSG_AUTH_REQUEST_CRYPT:
		case MSG_AUTH_REQUEST_GSS:
		case MSG_AUTH_REQUEST_GSS_CONT:
		case MSG_AUTH_REQUEST_SSPI:
		case MSG_AUTH_REQUEST_SASL:
		case MSG_AUTH_REQUEST_SASL_CONT:
		case MSG_AUTH_REQUEST_SASL_FINT:
			printf("* ERROR: unsupported auth: %i\n", auth_type);
			break;

		default:
			printf("* ERROR: unknown auth: %i\n", auth_type);
			break;
	}

	return false;
}
