/*****************************************************************************
 *
 * misc.c
 * Purpose ...............: Misc functions
 *
 *****************************************************************************
 * Copyright (C) 2013-2017 Robert James Clay <jame@rocasa.us>
 * Copyright (C) 1997-2007 Michiel Broek <mbse@mbse.eu>
 *
 * This file is part of FTNd.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * FTNd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with FTNd; see the file COPYING.  If not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/ftndlib.h"
#include "../lib/ftnd.h"
#include "../lib/users.h"
#include "../lib/msg.h"
#include "funcs.h"
#include "input.h"
#include "language.h"
#include "misc.h"
#include "timeout.h"
#include "exitinfo.h"
#include "ttyio.h"
#include "term.h"


extern char	*StartTime;	/* Time user logged in	    */

/*
 * Last caller action flags
 */
int		LC_Download = FALSE;
int		LC_Upload = FALSE;
int		LC_Read = FALSE;
int		LC_Wrote = FALSE;
int		LC_Chat = FALSE;
int		LC_Olr = FALSE;
int		LC_Door = FALSE;



int ChkFiles()
{
    FILE	*fp;
    char	*temp;
    time_t	Now;

    temp = calloc(PATH_MAX, sizeof(char));

    /*
     * Check if users.data exists, if not create a new one.
     */
    snprintf(temp, PATH_MAX, "%s/etc/users.data", getenv("FTND_ROOT"));
    if ((fp = fopen(temp,"rb")) == NULL) {
	if ((fp = fopen(temp,"wb")) == NULL) {
	    WriteError("$Can't create %s", temp);
	    ExitClient(FTNERR_INIT_ERROR); 
	} else {
	    usrconfighdr.hdrsize = sizeof(usrconfighdr);
	    usrconfighdr.recsize = sizeof(usrconfig);
	    fwrite(&usrconfighdr, sizeof(usrconfighdr), 1, fp);
	    fclose(fp);
	}
    } else {
	fclose(fp);
    }
    chmod(temp, 0660);

    /*
     * Check if sysinfo.data exists, if not, create a new one.
     */
    snprintf(temp, PATH_MAX, "%s/etc/sysinfo.data", getenv("FTND_ROOT"));
    if ((fp = fopen(temp, "rb")) == NULL) {
	if ((fp = fopen(temp, "wb")) == NULL) {
	    WriteError("$ChkFiles: Can't create %s", temp);
	} else {
	    memset((char *)&SYSINFO, 0, sizeof(SYSINFO));
	    Now = time(NULL);
	    SYSINFO.StartDate = Now;
	    fwrite(&SYSINFO, sizeof(SYSINFO), 1, fp);
	    fclose(fp);
	}
    } else {
	fclose(fp);
    }
    chmod(temp, 0660);

    free(temp);
    return 1;
}



void DisplayLogo()
{
    FILE	*pLogo;
    char	*sString, *temp;

    temp = calloc(PATH_MAX, sizeof(char));
    sString = calloc(1024, sizeof(char));

    snprintf(temp, PATH_MAX, "%s/share/int/txtfiles/%s/%s", getenv("FTND_ROOT"), CFG.deflang, CFG.welcome_logo);
    if ((pLogo = fopen(temp,"rb")) == NULL)
	WriteError("$DisplayLogo: Can't open %s", temp);
    else {
	while (Fgets(sString, 1023, pLogo) != NULL) {
	    PUTSTR(chartran(sString));
	    Enter(1);
	}
	fclose(pLogo);
    }

    free(sString);
    free(temp);
}



/*
 * Update a variable in the exitinfo file.
 */
void Setup(char *Option, char *variable)
{
    ReadExitinfo();
    strcpy(Option, variable);
    WriteExitinfo();
}



void SaveLastCallers()
{
    FILE	*pGLC;
    char	*sFileName, sFileDate[9], sDate[9];
    struct stat	statfile;

    /*
     * First check if we passed midnight, in that case we create a fresh file.
     */
    sFileName = calloc(PATH_MAX, sizeof(char));
    snprintf(sFileName, PATH_MAX, "%s/etc/lastcall.data", getenv("FTND_ROOT"));
    stat(sFileName, &statfile);

    snprintf(sFileDate, 9, "%s", StrDateDMY(statfile.st_mtime));
    snprintf(sDate, 9, "%s", (char *) GetDateDMY());

    if ((strcmp(sDate,sFileDate)) != 0) {
	unlink(sFileName);
	Syslog('+', "Erased old lastcall.data");
    }

    /*
     * Check if file exists, if not create the file and write the fileheader.
     */
    if ((pGLC = fopen(sFileName, "r")) == NULL) {
	if ((pGLC = fopen(sFileName, "w")) != NULL) {
	    LCALLhdr.hdrsize = sizeof(LCALLhdr);
	    LCALLhdr.recsize = sizeof(LCALL);
	    fwrite(&LCALLhdr, sizeof(LCALLhdr), 1, pGLC);
	    fclose(pGLC);
	    Syslog('+', "Created new lastcall.data");
	}
    } else {
	fclose(pGLC);
    }
    chmod(sFileName, 0660);

    /*
     * Now append a record
     */
    if ((pGLC = fopen(sFileName,"a+")) == NULL) {
	WriteError("$Can't open %s", sFileName);
	return;
    } else {
	ReadExitinfo();
	memset(&LCALL, 0, sizeof(LCALL));
	snprintf(LCALL.UserName, 36, "%s", exitinfo.sUserName);
	snprintf(LCALL.Handle, 36, "%s", exitinfo.sHandle);
	snprintf(LCALL.Name, 9, "%s", exitinfo.Name);
	snprintf(LCALL.TimeOn, 6, "%s", StartTime);
	snprintf(LCALL.Device, 10, "%s", pTTY);
	LCALL.SecLevel = exitinfo.Security.level;
	LCALL.Calls    = exitinfo.iTotalCalls;
	LCALL.CallTime = exitinfo.iConnectTime;
	LCALL.Download = LC_Download;
	LCALL.Upload   = LC_Upload;
	LCALL.Read     = LC_Read;
	LCALL.Wrote    = LC_Wrote;
	LCALL.Chat     = LC_Chat;
	LCALL.Olr      = LC_Olr;
	LCALL.Door     = LC_Door;
	snprintf(LCALL.Speed, 21, "%s", ttyinfo.speed);

	/* If true then set hidden so it doesn't display in lastcallers function */
	LCALL.Hidden = exitinfo.Hidden;

	snprintf(LCALL.Location, 28, "%s", exitinfo.sLocation);

	rewind(pGLC); /* ???????????? */
	fwrite(&LCALL, sizeof(LCALL), 1, pGLC);
	fclose(pGLC);
    }
    free(sFileName);
}



/* Gets Date for GetLastCallers(), returns DD:Mmm */
char *GLCdate()
{
    static char	GLcdate[15];

    Time_Now = time(NULL);
    l_date = localtime(&Time_Now);
    snprintf(GLcdate, 15, "%02d-", l_date->tm_mday);

    strcat(GLcdate,GetMonth(l_date->tm_mon+1));
    return(GLcdate);
}



