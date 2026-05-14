/*-------------------------------------------------------------------------
 * command.h
 *		Functionality for running commands in YellowedPlasticSQL
 *
 * Copyright (c) 2026, Daniel Gustafsson <daniel@yesql.se>
 *
 *-------------------------------------------------------------------------
 */
#ifndef _YP_COMMAND_H_
#define _YP_COMMAND_H_

#include "ypsql.h"

void command_run(YPconn *conn, char *cmd, int len);

#endif /* _YP_COMMAND_H_ */
