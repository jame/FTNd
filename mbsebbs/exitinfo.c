/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Exitinfo functions
 *
 *****************************************************************************
 * Copyright (C) 1997-2002
 *   
 * Michiel Broek		FIDO:		2:280/2802
 * Beekmansbos 10
 * 1971 BV IJmuiden
 * the Netherlands
 *
 * This file is part of MBSE BBS.
 *
 * This BBS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * MBSE BBS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MBSE BBS; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/libs.h"
#include "../lib/memwatch.h"
#include "../lib/mbse.h"
#include "../lib/structs.h"
#include "../lib/users.h"
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "funcs.h"
#include "input.h"
#include "language.h"
#include "oneline.h"
#include "misc.h"
#include "bye.h"
#include "timeout.h"
#include "timecheck.h"
#include "exitinfo.h"



/*
 * Copy usersrecord into ~/home/unixname/exitinfo
 */
int InitExitinfo()
{
    FILE    *pUsrConfig, *pExitinfo;
    char    *temp;
    long    offset;

    temp = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/etc/users.data", getenv("MBSE_ROOT"));

    if ((pUsrConfig = fopen(temp,"r+b")) == NULL) {
	WriteError("$Can't open %s for writing", temp);
	free(temp);
	return FALSE;
    }

    fread(&usrconfighdr, sizeof(usrconfighdr), 1, pUsrConfig);
    offset = usrconfighdr.hdrsize + (grecno * usrconfighdr.recsize);
    Syslog('b', "InitExitinfo: read users.data offset %ld", offset);
    if (fseek(pUsrConfig, offset, 0) != 0) {
	WriteError("$Can't move pointer in %s", temp);
	free(temp);
	return FALSE;
    }

    fread(&usrconfig, usrconfighdr.recsize, 1, pUsrConfig);

    exitinfo = usrconfig;
    fclose(pUsrConfig);

    sprintf(temp, "%s/%s/exitinfo", CFG.bbs_usersdir, usrconfig.Name);
    if ((pExitinfo = fopen(temp, "w+b")) == NULL) {
	WriteError("$Can't open %s for writing", temp);
	free(temp);
	return FALSE;
    } else {
	fwrite(&exitinfo, sizeof(exitinfo), 1, pExitinfo);
	fclose(pExitinfo);
	if (chmod(temp, 0600))
	    WriteError("$Can't chmod 0600 %s", temp);
    }
    free(temp);
    return TRUE;
}



/*
 * Function will re-read users file in memory, so the latest information
 * is available to other functions
 */
void ReadExitinfo()
{
    FILE *pExitinfo;
    char *temp;

    temp = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/%s/exitinfo", CFG.bbs_usersdir, sUnixName);
    mkdirs(temp, 0770);
    if ((pExitinfo = fopen(temp,"r+b")) == NULL)
	InitExitinfo();
    else {
	fflush(stdin);
	fread(&exitinfo, sizeof(exitinfo), 1, pExitinfo);
	fclose(pExitinfo);
    }
    free(temp);
}



/*
 * Function will rewrite userinfo from memory, so the latest information
 * is available to other functions
 */
void WriteExitinfo()
{
    FILE *pExitinfo;
    char *temp;

    temp = calloc(PATH_MAX, sizeof(char));

    sprintf(temp, "%s/%s/exitinfo", CFG.bbs_usersdir, sUnixName);
    if ((pExitinfo = fopen(temp,"w+b")) == NULL)
	WriteError("$WriteExitinfo() failed");
    else {
	fwrite(&exitinfo, sizeof(exitinfo), 1, pExitinfo);
	fclose(pExitinfo);
    }
    free(temp);
}


