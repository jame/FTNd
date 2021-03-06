/*****************************************************************************
 *
 * dirlock.c
 * Purpose ...............: Lock ftnfido processing.
 *
 *****************************************************************************
 * Copyright (C) 1997-2005 Michiel Broek <mbse@mbse.eu>
 * Copyright (C)    2013   Robert James Clay <jame@rocasa.us>
 *
 * This file is part of FTNd.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * FTNd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with FTNd; see the file COPYING.  If not, write to the Free
 * Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/ftndlib.h"
#include "flock.h"
#include "dirlock.h"

#define TMPNAME "TMP."
#define LCKNAME "LOCKFILE"



/*
 *  Put a lock on a directory.
 */
int lockdir(char *directory)
{
    char    *Tmpfile, *lockfile;
    FILE    *fp;
    pid_t   oldpid;

    Tmpfile  = calloc(PATH_MAX, sizeof(char));
    lockfile = calloc(PATH_MAX, sizeof(char));
    snprintf(Tmpfile, PATH_MAX, "%s/", directory);
    strcpy(lockfile, Tmpfile);
    snprintf(Tmpfile + strlen(Tmpfile), PATH_MAX, "%s%u", TMPNAME, getpid());
    snprintf(lockfile + strlen(lockfile), PATH_MAX - strlen(lockfile), "%s", LCKNAME);
	
    if ((fp = fopen(Tmpfile, "w")) == NULL) {
	WriteError("$Can't create lockfile \"%s\"", Tmpfile);
	free(Tmpfile);
	free(lockfile);
	return FALSE;
    }
    fprintf(fp, "%10u\n", getpid());
    fclose(fp);

    while (TRUE) {
	if (link(Tmpfile, lockfile) == 0) {
	    unlink(Tmpfile);
	    free(Tmpfile);
	    free(lockfile);
	    return TRUE;
	}
	if ((fp = fopen(lockfile, "r")) == NULL) {
	    WriteError("$Can't open lockfile \"%s\"", Tmpfile);
	    unlink(Tmpfile);
	    free(Tmpfile);
	    free(lockfile);
	    return FALSE;
	}
	if (fscanf(fp, "%u", &oldpid) != 1) {
	    WriteError("$Can't read old pid from \"%s\"", Tmpfile);
	    fclose(fp);
	    unlink(Tmpfile);
	    free(Tmpfile);
	    free(lockfile);
	    return FALSE;
	}
	fclose(fp);
	if (kill(oldpid,0) == -1) {
	    if (errno == ESRCH) {
		Syslog('+', "Stale lock found for pid %u", oldpid);
		unlink(lockfile);
		/* no return, try lock again */  
	    } else {
		WriteError("$Kill for %u failed",oldpid);
		unlink(Tmpfile);
		free(Tmpfile);
		free(lockfile);
		return FALSE;
	    }
	} else {
	    Syslog('+', "ftnfido already running, pid=%u", oldpid);
	    unlink(Tmpfile);
	    free(Tmpfile);
	    free(lockfile);
	    return FALSE;
	}
    }
}



/*
 * Unlock directory, make extra check to see if it is our own lock.
 */
void ulockdir(char *directory)
{
    char    *lockfile;
    FILE    *fp;
    pid_t   oldpid;

    lockfile = calloc(PATH_MAX, sizeof(char));
    snprintf(lockfile, PATH_MAX, "%s/", directory);
    snprintf(lockfile + strlen(lockfile), PATH_MAX - strlen(lockfile), "%s", LCKNAME);

    if ((fp = fopen(lockfile, "r")) == NULL) {
	/*
	 * No lockfile found, so not removed.
	 */
	free(lockfile);
	return;
    }

    if (fscanf(fp, "%u", &oldpid) != 1) {
	WriteError("$Can't read old pid from \"%s\"", lockfile);
    } else {
	if (getpid() != oldpid) {
	    WriteError("Attempt to remove lock %s of pid %d", lockfile, oldpid);
	} else {
	    /*
	     * Only remove our own lock.
	     */
	    unlink(lockfile);
	}
    }

    fclose(fp);
    free(lockfile);
}

