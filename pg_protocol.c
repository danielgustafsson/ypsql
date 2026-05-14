/*-------------------------------------------------------------------------
 * pg_protocol.c
 *		Implementation of the PostgreSQL v3 wire protocol
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "ypsql.h"
#include "net.h"
#include "pg_protocol.h"

static int prot_startup_message(YPconn *conn, char *packet);

/*
 * build_startup_message
 *
 * Allocate memory for, and build a startup message.  The construction is done
 * twice, first to calculate the number of bytes needed and then to construct
 * the actual message in the allocated buffer.  Returns NULL on failure.
 */
char *
build_startup_message(YPconn *conn, uint16_t *len)
{
	int startup_len;
	char *startup_packet;

	startup_len = prot_startup_message(conn, NULL);
	if (startup_len == 0)
		return NULL;

	*len = startup_len;
	startup_packet = (char *) malloc(*len);
	if (!startup_packet)
		return NULL;

	startup_len = prot_startup_message(conn, startup_packet);
	if (startup_len != *len)
	{
		free(startup_packet);
		return NULL;
	}

	return startup_packet;
}

/*
 * prot_startup_message
 *
 * Construct a StartupMessage in `packet` and return the number of bytes
 * written to it. If `packet` is NULL, it will still count the number of bytes
 * required for a StartupMessage and return but no message is constructed.
 */
static int
prot_startup_message(YPconn *conn, char *packet)
{
	int packet_len = sizeof(int32_t);
	int32_t pv = hton32(PROTOCOL_VERSION);

#define SET_OPTION(key, val) \
	do { \
		int foo = packet_len; \
		if (packet) \
		{ \
			strcpy(packet + packet_len, strlower(key)); \
			_a(packet + packet_len); \
		} \
		packet_len += strlen(key) + 1; \
		if (packet) \
		{\
			strcpy(packet + packet_len, strlower(val)); \
			_a(packet + packet_len); \
		} \
		packet_len += strlen(val) + 1; \
	} while(0)

	/* Protocol version */
	if (packet)
		memcpy(packet + packet_len, &pv, sizeof(int32_t));
	packet_len += sizeof(int32_t);

	/* 'user' is the only required option */
	SET_OPTION("user", conn->username);

	/* 'database' is for now hardcoded to the postgres database */
	SET_OPTION("database", "postgres");

	/* TODO: add application_name */

	/* Terminate the packet with a zero byte */
	if (packet)
		packet[packet_len] = '\0';
	packet_len++;

	/*
	 * First Int32 of the packet is the size of the packet in bytes, and by
	 * now we know what to write there.
	 */
	if (packet)
	{
		int32_t a = packet_len;
		a = hton32(a);
		memcpy(packet, &a, sizeof(int32_t));
	}

	return packet_len;
}

/*
 * read_ErrorResponse
 *
 * Read an ErrorResponse message from the input buffer.
 *
 * ErrorResponse is identified by a byte 'E' which is followed by an int32
 * containing the message length. Next follows a set of fields, each of which
 * consist of a byte indicating field type followed by a string.  The fields
 * and followed by a zero byte which is the terminator for the message.
 */
bool
read_ErrorResponse(YPconn *conn)
{
	char id;
	char err_str[128];

	/* TODO: Assert/validate msg_len */

	/* TODO: Read all fields. */
	while (true)
	{
		if (!net_read_byte(conn, &id))
			goto error;

		/* Terminator */
		if (id == '\0')
			break;

		if (id == PG_DIAG_SQLSTATE)
		{
			/* TODO save the sqlstate, not just discard it */
			net_read_string(conn, (char **) &err_str, sizeof(err_str));
			continue;
		}

		/*
		 * TODO: save the fields we need to construct an error message, make
		 * one and present it to the user.
		 */
		net_read_string(conn, (char **) &err_str, sizeof(err_str));
		printf("* E%i: %s\n", id, err_str);
	}

	return true;

error:
	return false;
}

/*
 * build_Query
 *
 * Construct a Query message for the SQL query passed as parameter. The Query
 * message is defined as follows:
 *
 * Byte1('Q')	# Identifier
 * Int32		# Length of message
 * String		# Query string
 */
YPstatus
build_Query(YPconn *conn, char *sql, char **msg, uint16_t *msg_len)
{
	int32_t packet_len;
	int32_t len;
	char *packet;

	(void) conn;

	packet_len = strlen(sql) + sizeof(int32_t) + 1 + 1;

	packet = malloc(packet_len + 1);
	if (!packet)
		return YP_ERROR_OOM;
	memset(packet, '\0', packet_len + 1);

	/* Identifier byte */
	packet[0] = MSG_QUERY;

	/* Packet length */
	len = hton32(packet_len);
	memcpy(packet + 1, &len, sizeof(int32_t));

	/* Query Text */
	strcpy(packet + 5, sql);
	_a(packet + 5);
	/* Add a trailing semicolon */
	packet[packet_len - 1] = 59;

	*msg = packet;
	*msg_len = packet_len + 1;

	return YP_OK;
}

/*
 * build_PasswordMessage
 *
 * Construct a Password message for authentication either via password or MD5
 * auth schemes. The Password message is defined as follows:
 *
 * Byte1('p')		# Identifier
 * Int32			# Length of message
 * String			# The password, encrypted or plaintext depending on scheme
 */
YPstatus
build_PasswordMessage(YPconn *conn, char **msg, uint16_t *msg_len)
{
	int packet_len = 0;
	int32_t len = 0;
	char *packet = NULL;

	packet_len = strlen(conn->password) + sizeof(int32_t) + 1;

	packet = malloc(packet_len + 1);
	if (!packet)
		return YP_ERROR_OOM;
	memset(packet, '\0', packet_len + 1);

	/* Identifier byte */
	packet[0] = MSG_PASSWORD;

	/* Packet length */
	len = hton32(packet_len);
	memcpy(packet + 1, &len, sizeof(int32_t));

	/* Password payload */
	strcpy(packet + 5, conn->password);
	_a(packet + 5);

	*msg = packet;
	*msg_len = packet_len + 1;

	return YP_OK;
}

/*
 * read_AuthenticationOk
 *
 * Currently we just consume the data without any action on the content. The
 * AuthenticationOk message is defined as follows:
 *
 * Byte1('R')			# Identifier
 * Int32(8)				# Message Length
 * Int32(0)				# Static value indicating success
 */
YPstatus
read_AuthenticationOk(YPconn *conn)
{
	int32_t status;

	net_skip_int(conn, 4);
	net_read_int32(conn, &status);

	return YP_OK;
}

/*
 * read_BackendKeyData
 * Currently we just consume the data without any action on the content. When
 * support for query cancellation is added this needs to be implemented. The
 * BackendKeyData  message is defined as follows:
 *
 * Byte1('K')			# Identifier
 * Int32				# Message Length
 * Int32				# Backend PID
 * Byten				# n bytes containing the secret key, the value of n
 *						# is defined by the message length
 */
YPstatus
read_BackendKeyData(YPconn *conn)
{
	int32_t msg_len;

	net_read_int32(conn, &msg_len);
	net_skip_int(conn, 4);
	net_skip_n_bytes(conn, (uint16_t)(msg_len - 8));
	return YP_OK;
}

YPstatus
read_ParameterStatus(YPconn *conn)
{
	int32_t msg_len;
	net_read_int32(conn, &msg_len);
	net_skip_n_bytes(conn, (uint16_t)(msg_len - 4));
	return YP_OK;
}

YPstatus
read_NoticeResponse(YPconn *conn)
{
	net_skip_int(conn, 4);
	net_skip_byte(conn);
	/* TODO replace with net_skip_n_bytes */
	net_skip_string(conn);
	return YP_OK;
}

YPstatus
read_CommandComplete(YPconn *conn)
{
	int32_t		msg_len;
	char		tag[16];
	int			i;

	net_read_int32(conn, &msg_len);

	for (i = 0; i < msg_len - 4; i++)
	{
		if (i > sizeof(tag) - 1)
		{
			net_skip_byte(conn);
		}
		else
		{
			net_read_byte(conn, &tag[i]);
		}
	}
	_p(tag);
	printf("%s\n", tag);
	return YP_OK;
}

/*
 * read_rowDescription
 *
 * Byte1('T')		# Identifier
 * Int32			# Length of message
 * Int16			# The number of fields following (can be zero)
 *	String			# Field name
 *	Int32			# If a column, the Oid of the table, else zero
 *	Int16			# If a column, the attribyte number, else zero
 *	Int32			# Oid of the field data type
 *	Int16			# Size of the date type, negative for variable-width
 *	Int32			# The type modifier
 *	Int16			# Fornat code, 0 means text and 1 means binary
 */
YPstatus
read_RowDescription(YPconn *conn)
{
	int32_t	msg_len;
	int16_t	fields;
	int16_t	intval;
	char str[64];

	/* Message length */
	net_read_int32(conn, &msg_len);

	net_read_int16(conn, &fields);

	while (fields > 0)
	{
		int i = 0;
		char c;
		memset(str, '\0', sizeof(str));

		do
		{
			net_read_byte(conn, &c);
			if (i < sizeof(str) - 1)
				str[i++] = c;
		} while (c != '\0');

		net_skip_int(conn, 4);
		net_read_int16(conn, &intval);

		if (intval != 0)
			printf(": %s ", str);

		/* Advance the read pointer past fields we don't need */
		net_skip_int(conn, 4);
		net_skip_int(conn, 2);
		net_skip_int(conn, 4);
		net_skip_int(conn, 2);

		fields--;
	}
	printf(":\n");

	return YP_OK;
}

/*
 * read_DataRow
 *
 * Read a DataRow and display it to the user. The DataRow message is defined
 * as follows:
 *
 * Byte1('D')			# Identifier
 * Int32				# Message length
 * Int16				# Number of attributes following, can be zero
 *	Int32				# Length of the column value, -1 indicates NULL
 *	Byten				# Value if the column, whrere n is the above Int32
 */
YPstatus
read_DataRow(YPconn *conn)
{
	int32_t	msg_length = 0;
	int16_t	columns = 0;
	int32_t	col_width = 0;
	char		col_data[128];

	net_read_int32(conn, &msg_length);
	net_read_int16(conn, &columns);

	while (columns > 0)
	{
		int i;
		net_read_int32(conn, &col_width);
		memset(col_data, '\0', sizeof(col_data));

		for (i = 0; i < col_width; i++)
		{
			if (i > sizeof(col_data) - 1)
			{
				net_skip_byte(conn);
			}
			else
			{
				net_read_byte(conn, &col_data[i]);
			}
		}

		printf(" %s ", col_data);

		columns--;
	}
	printf("\n");

	return YP_OK;
}

/*
 * read_ReadyForQuery
 *
 * The ReadyForQuery message is the final message sent during query processing
 * and will be transmitted regardless of whether the query errored out or not.
 * The ReadyForQuery message is defined as follows:
 *
 * Byte1('Z')			# Identifier
 * Int32(5)				# Message length
 * Byte1				# Transaction status indicator, 'I' = idle, 'T' = in
 *						# transaction, 'E' = in failed transaction.
 */
YPstatus
read_ReadyForQuery(YPconn *conn)
{
	/*
	 * For now we don't process the ReadyForQuery message contents, a TODO is
	 * to reflect the transaction status in the prompt.
	 */
	net_skip_int(conn, 4);
	net_skip_byte(conn);

	return YP_OK;
}
