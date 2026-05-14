/*-------------------------------------------------------------------------
 * pg_protocol.h
 *		Implementation of the PostgreSQL v3 wire protocol
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PG_PROTOCOL_H_
#define _PG_PROTOCOL_H_

#include "ypsql.h"

#define PROTOCOL_VERSION 0x0030002

/*---------------------------------------------------------------------------
 * Authentication
 *-------------------------------------------------------------------------*/
#define MSG_AUTH_REQUEST				82	/* 'R', AuthenticationOk */
/* Authentication request types */
#define MSG_AUTH_REQUEST_OK				0	/* User is authenticate */
#define MSG_AUTH_REQUEST_KRB4			1	/* Kerberos v4. Not supported. */
#define MSG_AUTH_REQUEST_KRB5			2	/* Kerberos v5. Not supported.  */
#define MSG_AUTH_REQUEST_PASSWORD		3	/* Password auth */
#define MSG_AUTH_REQUEST_CRYPT			4	/* Crypt password. Not supported. */
#define MSG_AUTH_REQUEST_MD5			5	/* MD5 password. */
#define MSG_AUTH_REQUEST_GSS			7 	/* GSSAPI. Not supported. */
#define MSG_AUTH_REQUEST_GSS_CONT		8	/* Continue GSSAPI. Not supported. */
#define MSG_AUTH_REQUEST_SSPI			9	/* SSPI negotiate. Not supported. */
#define MSG_AUTH_REQUEST_SASL		   10	/* Begin SASL. Not supported. */
#define MSG_AUTH_REQUEST_SASL_CONT	   11	/* Continue SASL. Not supported. */
#define MSG_AUTH_REQUEST_SASL_FINT	   12	/* Final SASL. Not supported. */

#define MSG_PASSWORD				  112	/* 'p', PasswordMessage */
/*---------------------------------------------------------------------------
 * Error and Notice reporting
 *-------------------------------------------------------------------------*/
#define MSG_ERROR_RESPONSE				69		/* 'E', ErrorResponse */
/*
 * ErrorResponse message identifiers. Most of these are discarded due to the
 * UI being limited and not yet complete, adding support for more of these is
 * a TODO.
 */
#define PG_DIAG_SEVERITY				 83		/* 'S' */
#define PG_DIAG_SEVERITY_NONLOCALIZED	 86		/* 'V' */
#define PG_DIAG_SQLSTATE				 67		/* 'C' */
#define PG_DIAG_MESSAGE_PRIMARY			 77		/* 'M' */
#define PG_DIAG_MESSAGE_DETAIL			 68		/* 'D' */
#define PG_DIAG_MESSAGE_HINT			 72		/* 'H' */
#define PG_DIAG_STATEMENT_POSITION		 80		/* 'P' */
#define PG_DIAG_INTERNAL_POSITION		112		/* 'p' */
#define PG_DIAG_INTERNAL_QUERY			113		/* 'q' */
#define PG_DIAG_CONTEXT					 87		/* 'W' */
#define PG_DIAG_SCHEMA_NAME				116		/* 's' */
#define PG_DIAG_TABLE_NAME				117		/* 't' */
#define PG_DIAG_COLUMN_NAME				 99		/* 'c' */
#define PG_DIAG_DATATYPE_NAME			100		/* 'd' */
#define PG_DIAG_CONSTRAINT_NAME			110		/* 'n' */
#define PG_DIAG_SOURCE_FILE				 70		/* 'F' */
#define PG_DIAG_SOURCE_LINE				 76		/* 'L' */
#define PG_DIAG_SOURCE_FUNCTION			 82		/* 'R' */

#define MSG_NOTICE_RESPONSE				 78		/* 'N', NoticeResponse */

/*---------------------------------------------------------------------------
 * Protocol and Connection Control
 *-------------------------------------------------------------------------*/
#define MSG_BACKEND_KEY_DATA			 75		/* 'K', BackendKeyData */
#define MSG_PARAMETER_STATUS			 83		/* 'S', ParameterStatus */
#define MSG_NEGOTIATE_PROTOCOL_VERSION	118		/* 'v', NegotiateProtocolVersion */
/* Protocol Negotation error codes */
#define ERRCODE_CANNOT_CONNECT_NOW		"57P03"
#define ERRCODE_PROTOCOL_VIOLATION		"08P01"


/*-------------------------------------------------------------------------
 * Authentication
 *------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------
 * Query Processing
 *------------------------------------------------------------------------*/
#define MSG_QUERY					81		/* 'Q', Query Message */
#define MSG_ROW_DESCRIPTION			84		/* 'T', RowDescription Message */
#define MSG_DATA_ROW				68		/* 'D', DataRow Message */
#define MSG_READY_FOR_QUERY			90		/* 'Z', ReadyForQuery Message */
#define MSG_COMMAND_COMPLETE		67		/* 'C', CommandComplete Message */
#define MSG_COPYIN_RESPONSE			71		/* 'G', CopyInResponse */
#define MSG_COPYOUT_RESPONSE		72		/* 'H', CopyOutResponse */
#define MSG_EMPTY_QUERY_RESPONSE	73		/* 'I', EmptyQueryResponse */

char *build_startup_message(YPconn *conn, uint16_t *len);

bool read_ErrorResponse(YPconn *conn);
YPstatus build_Query(YPconn *conn, char *sql, char **msg, uint16_t *msg_len);
YPstatus build_PasswordMessage(YPconn *conn, char **msg, uint16_t *msg_len);
YPstatus read_DataRow(YPconn *conn);
YPstatus read_RowDescription(YPconn *conn);
YPstatus read_ReadyForQuery(YPconn *conn);
YPstatus read_AuthenticationOk(YPconn *conn);
YPstatus read_BackendKeyData(YPconn *conn);
YPstatus read_ParameterStatus(YPconn *conn);
YPstatus read_NoticeResponse(YPconn *conn);
YPstatus read_CommandComplete(YPconn *conn);

#endif /* _PG_PROTOCOL_H_ */
