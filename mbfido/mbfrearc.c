/*****************************************************************************
 *
 * $Id$
 * Purpose: File Database Maintenance - ReArc file(s)
 *
 *****************************************************************************
 * Copyright (C) 1997-2004
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
 * Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/mbselib.h"
#include "../lib/users.h"
#include "../lib/mbsedb.h"
#include "mbfutil.h"
#include "mbfmove.h"



extern int	do_quiet;		/* Suppress screen output	    */



/*
 * ReArc file(s)
 */
void ReArc(int Area, char *File)
{
    char	    *p, *temp, *linkpath, mask[256];
    FILE	    *fp;
    int		    i, rc = -1;
    struct utimbuf  ut;

    IsDoing("ReArc file(s)");
    colour(LIGHTRED, BLACK);

    /*
     * Check area
     */
    if (LoadAreaRec(Area) == FALSE) {
	WriteError("Can't load record %d", Area);
	die(MBERR_INIT_ERROR);
    }
    if (!area.Available) {
	WriteError("Area %d not available", Area);
	if (!do_quiet)
	    printf("Area %d not available\n", Area);
	die(MBERR_CONFIG_ERROR);
    }
    if (area.CDrom) {
	WriteError("Can't rearc on CD-ROM");
	if (!do_quiet)
	    printf("Can't rearc on CD-ROM\n");
	die(MBERR_COMMANDLINE);
    }
    if (strlen(area.Archiver) == 0) {
	WriteError("No default archiver for area %d", Area);
	if (!do_quiet)
	    printf("No default archiver for area %d\n", Area);
	die(MBERR_COMMANDLINE);
    }
    if (CheckFDB(Area, area.Path))
	die(MBERR_GENERAL);

    temp = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/fdb/file%d.data", getenv("MBSE_ROOT"), Area);

    if ((fp = fopen(temp, "r+")) == NULL)
	die(MBERR_GENERAL);

    fread(&fdbhdr, sizeof(fdbhdr), 1, fp);
    colour(CYAN, BLACK);
    strcpy(mask, re_mask(File, FALSE));
    if (re_comp(mask))
	die(MBERR_GENERAL);

    while (fread(&fdb, fdbhdr.recsize, 1, fp) == 1) {
	if (re_exec(fdb.LName) || re_exec(fdb.Name)) {
	    Syslog('+', "Will rearc %s", fdb.LName);
	    sprintf(temp, "%s/%s", area.Path, fdb.Name);
	    
	    rc = rearc(temp, area.Archiver, do_quiet);
	    if (rc == 0) {
		/*
		 * Success, update the file entry
		 */
    		if (!do_quiet) {
		    colour(9, 0);
		    printf("\r   Update file %s", temp);
		    fflush(stdout);
		}

                linkpath = calloc(PATH_MAX, sizeof(char));
		sprintf(linkpath, "%s/%s", area.Path, fdb.LName);
		unlink(linkpath);

		Syslog('+', "New name %s", temp);
		if ((p = strstr(fdb.Name, "ARC")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "LHA")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "RAR")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "TGZ")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "BZ2")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "TAR")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "ARJ")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "ZIP")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "ZOO")))
		    *p = '\0';
		else if ((p = strstr(fdb.Name, "HA")))
		    *p = '\0';
		sprintf(p, "%s", archiver.name);
		if ((p = strstr(fdb.LName, "arc")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "lha")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "rar")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "tar.gz")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "tgz")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "tar.bz2")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "bz2")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "tar")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "arj")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "zip")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "zoo")))
		    *p = '\0';
		else if ((p = strstr(fdb.LName, "ha")))
		    *p = '\0';
		sprintf(p, "%s", tl(archiver.name));
		Syslog('f', "%s %s", fdb.Name, fdb.LName);
		fdb.Size = file_size(temp);
		fdb.Crc32 = file_crc(temp, FALSE);
		ut.actime = mktime(localtime(&fdb.FileDate));
		ut.modtime = mktime(localtime(&fdb.FileDate));
		utime(temp, &ut);
		fseek(fp, - fdbhdr.recsize, SEEK_CUR);
		fwrite(&fdb, fdbhdr.recsize, 1, fp);

		/*
		 * Update symbolic link to long filename
		 */
		sprintf(linkpath, "%s/%s", area.Path, fdb.LName);
		symlink(temp, linkpath);
		free(linkpath);
	    } else {
		break; // stop when something goes wrong
	    }
	    if (!do_quiet) {
		colour(7, 0);
		printf("\r");
		for (i = 0; i < (strlen(temp) + 17); i++)
		    printf(" ");
		printf("\r");
		fflush(stdout);
	    }
	}
    }
    fclose(fp);
    free(temp);
}

