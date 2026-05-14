/*-------------------------------------------------------------------------
 * query.h
 *		Functionality for sending queries to PostgreSQL, and processing any
 *		results.
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#ifndef _YP_QUERY_H_
#define _YP_QUERY_H_

#include "ypsql.h"

YPstatus send_query(YPconn *conn, char *sql);

#endif	/* _YP_QUERY_H_ */
