/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: All the file sub functions. 
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
#include "../lib/mbse.h"
#include "../lib/users.h"
#include "../lib/mbsedb.h"
#include "filesub.h"
#include "funcs.h"
#include "language.h"
#include "input.h"
#include "misc.h"
#include "timeout.h"
#include "exitinfo.h"
#include "change.h"


extern pid_t	    mypid;
long		    arecno = 1;	/* Area record number			     */
int		    Hcolor = 9;	/* Color of area line in xxxScan() functions */


/*
 * Variables for file tagging
 */
int	Tagnr;
_Tag	Tagbuf[100];



/*
 * Reset the tag ringbuffer.
 */
void InitTag()
{
	int	i;

	Tagnr = 0;

	for (i = 0; i < 100; i++) {
		memset(&Tagbuf[i], 0, sizeof(_Tag));
	}
}



/*
 * Add a file in the tag ringbuffer.
 */
void SetTag(_Tag tag)
{
	if (Tagnr < 99)
		Tagnr++;
	else
		Tagnr = 1;

	Tagbuf[Tagnr] = tag;
}



int ForceProtocol()
{
	/*
	 * If user has no default protocol, make sure he has one.
	 */
	if (strcmp(sProtName, "") == 0) {
		Chg_Protocol();

		/*
		 * If the user didn't pick a protocol, quit.
		 */
		if (strcmp(sProtName, "") == 0) {
			return FALSE;
		}
	}
	return TRUE;
}



/*
 * Get string, no newline afterwards.
 */
void GetstrD(char *sStr, int iMaxlen)
{
	unsigned char	ch = 0;
	int		iPos = 0;

	fflush(stdout);

	if ((ttyfd = open ("/dev/tty", O_RDWR)) < 0) {
		perror("open 6");
		return;
	}
	Setraw();
	strcpy(sStr, "");

	alarm_on();
	while (ch != 13) {
		ch = Readkey();

		if (((ch == 8) || (ch == KEY_DEL) || (ch == 127)) && (iPos > 0)) {
			printf("\b \b");
			fflush(stdout);
			sStr[--iPos]='\0';
		}

		if (ch > 31 && ch < 127) {
			if (iPos <= iMaxlen) {
				iPos++;
				sprintf(sStr, "%s%c", sStr, ch);
				printf("%c", ch);
				fflush(stdout);
			} else
				ch=07;
		}
	}

	Unsetraw();
	close(ttyfd);
}



/*
 * Open FileDataBase, the filepointer is at the begin of the first record.
 */
#ifndef	USE_EXPERIMENT
FILE *OpenFileBase(unsigned long Area, int Write)
{
    FILE    *pFile;
    char    *FileBase;

    FileBase = calloc(PATH_MAX, sizeof(char));
    sprintf(FileBase,"%s/fdb/file%ld.data", getenv("MBSE_ROOT"), Area);

    if (Write)
	pFile = fopen(FileBase, "r+");
    else
	pFile = fopen(FileBase, "r");

    if (pFile == NULL) {
	WriteError("$Can't open file: %s", FileBase);
	/* Can't open file database for this area */
	printf("%s\n\n", (char *) Language(237));
	sleep(2);
    }

    fread(&fdbhdr, sizeof(fdbhdr), 1, pFile);
    free(FileBase);
    return pFile;
}
#endif


/*
 * Open the fareas.data file for read or R/W and read the headerrecord.
 * The filepointer is at the start of the first record.
 */
FILE *OpenFareas(int Write)
{
	FILE	*pAreas;
	char	*FileArea;

	FileArea = calloc(PATH_MAX, sizeof(char));
	sprintf(FileArea, "%s/etc/fareas.data", getenv("MBSE_ROOT"));
		
	if (Write)
		pAreas = fopen(FileArea, "r+");
	else
		pAreas = fopen(FileArea, "r");
		
	if (pAreas == NULL) {
		WriteError("$Can't open FileBase %s", FileArea);
		/* FATAL: Unable to open areas database */
		printf("%s\n\n", (char *) Language(243));
		sleep(2);
	} else
	 	fread(&areahdr, sizeof(areahdr), 1, pAreas);

	free(FileArea); 	
 	return pAreas;
}



/*
 * Pageheader for filelistings
 */
void Header()
{
	colour(4, 7);
	printf(" Area ");

	colour(4, 7);
	printf("%-5d   ", iAreaNumber);

	colour(1,7);
	printf("%-65s\n", sAreaDesc);

	colour(15,0);
	fLine(79);
}



/*
 * Searchheader for areas during xxxxScan().
 */
void Sheader()
{
	colour(Hcolor, 0);
	printf("\r  %-4ld", arecno);

	colour(9, 0);
	printf(" ... ");

	colour(Hcolor, 0);
	printf("%-44s", area.Name);
	fflush(stdout);

	if (Hcolor < 15)
		Hcolor++;
	else
		Hcolor = 9;
}



/*
 * Blank current line without newline.
 */
void Blanker(int count)
{
	int i;

	for (i = 0; i < count; i++)
		printf("\b");

	for (i = 0; i < count; i++)
		printf(" ");

	printf("\r");
	fflush(stdout);
}



/*
 * Mark one or more files for download by putting them into the "taglist"
 * in the users homedirectory. Check against dupe tags.
 */
void Mark()
{
	char	*temp;
	FILE	*fp;
	int	i, Found;
	int	Count, Size;

	temp = calloc(81, sizeof(char));

	/*
	 * First count the already tagged files.
	 */
	Count = Size = 0;
	if ((fp = fopen("taglist", "r")) != NULL) {
		while (fread(&Tag, sizeof(Tag), 1, fp) == 1) {
			if (Tag.Active) {
				Count++;
				Size += (Tag.Size / 1024);
			}
		}
		fclose(fp);
	}

	colour(CFG.HiliteF, CFG.HiliteB);
	/* Marked: */
	printf("%s%d, %dK; ", (char *) Language(360), Count, Size);

	/* Mark file number of press <Enter> to stop */
	printf("%s", (char *) Language(7));

	colour(CFG.InputColourF, CFG.InputColourB);
	GetstrD(temp, 10);
	Blanker(strlen(Language(7)) + strlen(temp));

	if (strlen(temp) == 0) {
		free(temp);
		return;
	}

	i = atoi(temp);

	if ((i > 0) && (i < 100)) {
		if ((Tagbuf[i].Area) && (strlen(Tagbuf[i].LFile))) {
			if (Access(exitinfo.Security, area.DLSec)) {
				if ((fp = fopen("taglist", "a+")) != NULL) {

					fseek(fp, 0, SEEK_SET);
					Found = FALSE;
					while (fread(&Tag, sizeof(Tag), 1, fp) == 1)
						if ((Tag.Area == Tagbuf[i].Area) && (strcmp(Tag.LFile, Tagbuf[i].LFile) == 0)) {
							Found = TRUE;
							Syslog('b', "Tagbuf[i].File already tagged");
						}

					if (!Found) {
						memset(&Tag, 0, sizeof(Tag));
						Tag = Tagbuf[i];
						Tag.Active = TRUE;
						fwrite(&Tag, sizeof(Tag), 1, fp);
						Syslog('+', "Tagged file %s from area %d", Tag.LFile, Tag.Area);
					}

					fclose(fp);
				}
			} else {
				colour(12, 0);
				/* You do not have enough access to download from this area. */
				printf("%s", (char *) Language(244));
				fflush(stdout);
				sleep(3);
				Blanker(strlen(Language(244)));
			}
		}
	}

	free(temp);
}



/* 
 * More prompt, returns 1 if user decides not to look any further.
 */
int iLC(int Lines)
{
	int	x, z;

	x = strlen(Language(131));
	iLineCount += Lines;
 
	if ((iLineCount >= exitinfo.iScreenLen) && (iLineCount < 1000)) {
		iLineCount = 0;

		while(TRUE) {
			/* More (Y/n/=) M=Mark */
			pout(CFG.MoreF, CFG.MoreB, (char *) Language(131));

			fflush(stdout);
			alarm_on();
			z = toupper(Getone());
			Blanker(x);

			if (z == Keystroke(131, 1)) {
				printf("\n");
				return 1;
			}

			if (z == Keystroke(131, 2)) {
				iLineCount = 1000;
				return 0;
			}

			if ((z == Keystroke(131, 0)) || (z == '\r') || (z == '\n')) {
				return 0;
			}

			if (z == Keystroke(131, 3)) {
				Mark();
			}
		}
	}
	return 0;
}



/*
 * Show one file, return 1 if user wants to stop, 0 to show next file.
 */
int ShowOneFile()
{
    int	y, z, fg, bg;

    if (!fdb.Deleted) {

	colour(7, 0);
	printf(" %02d ", Tagnr);

	colour(CFG.FilenameF, CFG.FilenameB);
	printf("%-12s", fdb.Name);

	colour(CFG.FilesizeF, CFG.FilesizeB);
	printf("%10lu ", (long)(fdb.Size));

	colour(CFG.FiledateF, CFG.FiledateB);
	printf("%-10s  ", StrDateDMY(fdb.UploadDate));

	colour(12, 0);
	printf("[%4ld] ", fdb.TimesDL);

	if ((strcmp(fdb.Uploader, "")) == 0)
	    strcpy(fdb.Uploader, "SysOp");

	colour(CFG.HiliteF, CFG.HiliteB);
	printf("%s%s\n", (char *) Language(238), fdb.Uploader);

	if (iLC(1) == 1) 
	    return 1;

	for (z = 0; z < 25; z++) {
	    if ((y = strlen(fdb.Desc[z])) > 1) {
		if ((fdb.Desc[z][0] == '@') && (fdb.Desc[z][1] == 'X')) {
		    /*
		     *  Color formatted description lines.
		     */
		    if (fdb.Desc[z][3] > '9')
			fg = (int)fdb.Desc[z][3] - 55;
		    else
			fg = (int)fdb.Desc[z][3] - 48;
		    bg = (int)fdb.Desc[z][2] - 48;
		    colour(fg, bg);
		    printf("    %s\n",fdb.Desc[z]+4);
		} else {
		    colour(CFG.FiledescF, CFG.FiledescB);
		    printf("    %s\n",fdb.Desc[z]);
		}

		if (iLC(1) == 1) 
		    return 1;
	    }
	}
    }
    return 0;
}



int CheckBytesAvailable(long CostSize)
{
    if (LIMIT.DownK) {
	if ((exitinfo.DownloadKToday <= 0) || ((CostSize / 1024) > exitinfo.DownloadKToday)) {
	
	    /* You do not have enough bytes to download \" */
	    pout(LIGHTRED, BLACK, (char *) Language(252));
	    Enter(1);
	    Syslog('+', "Not enough bytes to download %ld", CostSize);

	    colour(WHITE, BLACK);
	    /* You must upload before you can download. */
	    pout(LIGHTRED, BLACK, (char *) Language(253));
	    Enter(2);

	    colour(YELLOW, BLACK);
	    /* Kilobytes currently available: */
	    printf("%s%lu Kbytes.\n\n", (char *) Language(254), exitinfo.DownloadKToday);

	    Pause();
	    return FALSE;
	}
    }
	
    return TRUE;
}



/*
 * Change back to users homedir.
 */
void Home()
{
    char    *temp;

    temp = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/%s", CFG.bbs_usersdir, exitinfo.Name);
    chdir(temp);
    free(temp);
}



/*
 * Scan a .COM or .EXE file in users upload directory.
 */
int ScanDirect(char *fn)
{
    FILE    *fp, *lp;
    int	    err, Found = FALSE;
    char    *temp, *temp1, *stdlog, *errlog, buf[256];

    temp  = calloc(PATH_MAX, sizeof(char));
    temp1 = calloc(PATH_MAX, sizeof(char));
    stdlog = calloc(PATH_MAX, sizeof(char));
    errlog = calloc(PATH_MAX, sizeof(char));
    
    sprintf(temp, "%s/%s/upl/%s", CFG.bbs_usersdir, exitinfo.Name, fn);
    sprintf(temp1, "%s/etc/virscan.data", getenv("MBSE_ROOT"));
    sprintf(stdlog, "%s/tmp/stdlog%d", getenv("MBSE_ROOT"), mypid);
    sprintf(errlog, "%s/tmp/errlog%d", getenv("MBSE_ROOT"), mypid);

    if ((fp = fopen(temp1, "r")) != NULL) {
	fread(&virscanhdr, sizeof(virscanhdr), 1, fp);

	while (fread(&virscan, virscanhdr.recsize, 1, fp) == 1) {

	    if (virscan.available) {
		colour(CFG.TextColourF, CFG.TextColourB);
				      /* Scanning */               /* with */
		printf("%s %s %s %s ", (char *) Language(132), fn, (char *) Language(133), virscan.comment);
		fflush(stdout);

		Altime(3600);
		err = execute_str(virscan.scanner, virscan.options, temp, (char *)"/dev/null", stdlog, errlog);
		if (file_size(stdlog)) {
		    if ((lp = fopen(stdlog, "r"))) {
			while (fgets(buf, sizeof(buf) -1, lp)) {
			    Striplf(buf);
			    Syslog('+', "stdout: \"%s\"", printable(buf, 0));
			}
			fclose(lp);
		    }
		}
		if (file_size(errlog)) {
		    if ((lp = fopen(errlog, "r"))) {
			while (fgets(buf, sizeof(buf) -1, lp)) {
			    Striplf(buf);
			    Syslog('+', "stderr: \"%s\"", printable(buf, 0));
    			}
			fclose(lp);
		    }
		}
		unlink(stdlog);
		unlink(errlog);
		if (err != virscan.error) {
		    WriteError("VIRUS ALERT: Result %d (%s)", err, virscan.comment);
		    colour(CFG.HiliteF, CFG.HiliteB);
		    /* Possible VIRUS found! */
		    printf("%s\n", (char *) Language(199));
		    Found = TRUE;
		} else {
		    /* Ok */
		    printf("%s\n", (char *) Language(200));
		}
		Altime(0);
		Nopper();
		fflush(stdout);
	    }
	}
	fclose(fp);
    }

    free(temp);
    free(temp1);
    free(stdlog);
    free(errlog);
    return Found;
}



/*
 * Scan archive using users ./tmp directory.
 * Return codes:
 *   0 - All seems well
 *   1 - Error unpacking archive
 *   2 - Possible virus found
 *   3 - Not a known archive format.
 */
int ScanArchive(char *fn, char *ftype)
{
    FILE    *fp, *lp;
    int	    err = 0, Found = FALSE;
    char    *temp, *stdlog, *errlog, buf[256];
    char    *cwd = NULL;


    /*
     * First search for the right archiver program
     */
    temp = calloc(PATH_MAX, sizeof(char));
    stdlog = calloc(PATH_MAX, sizeof(char));
    errlog = calloc(PATH_MAX, sizeof(char));
	
    sprintf(temp, "%s/etc/archiver.data", getenv("MBSE_ROOT"));
    sprintf(stdlog, "%s/tmp/stdlog%d", getenv("MBSE_ROOT"), mypid);
    sprintf(errlog, "%s/tmp/errlog%d", getenv("MBSE_ROOT"), mypid);
	
    if ((fp = fopen(temp, "r")) == NULL) {
	free(temp);
	return 3;
    }

    fread(&archiverhdr, sizeof(archiverhdr), 1, fp);

    while (fread(&archiver, archiverhdr.recsize, 1, fp) == 1) {
	if ((strcmp(ftype, archiver.name) == 0) && (archiver.available)) {
	    break;
	}
    }
    fclose(fp);
    if ((strcmp(ftype, archiver.name)) || (!archiver.available)) {
	free(temp);
	return 3;
    }

    cwd = getcwd(cwd, 80);
    sprintf(temp, "%s/%s/tmp", CFG.bbs_usersdir, exitinfo.Name);
    if (chdir(temp)) {
	WriteError("$Can't chdir(%s)", temp);
	free(temp);
	return 1;
    }

    colour(CFG.TextColourF, CFG.TextColourB);
    /* Unpacking archive */
    printf("%s %s ", (char *) Language(201), fn);
    fflush(stdout);

    if (!strlen(archiver.funarc)) {
	WriteError("No unarc command available");
    } else {
	sprintf(temp, "%s/%s/upl/%s", CFG.bbs_usersdir, exitinfo.Name, fn);
	if (execute_str(archiver.funarc, temp, (char *)NULL, (char *)"/dev/null", (char *)"/dev/null", (char *)"/dev/null")) {
	    WriteError("$Failed %s %s", archiver.funarc, temp);
	    execute_pth((char *)"rm", (char *)"-r -f ./*", (char *)"/dev/null", (char *)"/dev/null", (char *)"/dev/null");
	    chdir(cwd);
	    free(cwd);
	    colour(CFG.HiliteF, CFG.HiliteB);
	    /* ERROR */
	    printf("%s\n", (char *) Language(217));
	    fflush(stdout);
	    return 1;
	}
    }

    /* Ok */
    printf("%s\n", (char *) Language(200));
    fflush(stdout);

    sprintf(temp, "%s/etc/virscan.data", getenv("MBSE_ROOT"));

    if ((fp = fopen(temp, "r")) != NULL) {
	fread(&virscanhdr, sizeof(virscanhdr), 1, fp);
	while (fread(&virscan, virscanhdr.recsize, 1, fp) == 1) {

	    if (virscan.available) {
		colour(CFG.TextColourF, CFG.TextColourB);
				    /* Scanning */		   /* with */
		printf("%s %s %s %s ", (char *) Language(132), fn, (char *) Language(133), virscan.comment);
		fflush(stdout);

		Altime(3600);
		err = execute_str(virscan.scanner, virscan.options, (char *)"*", (char *)"/dev/null", stdlog, errlog);
		if (file_size(stdlog)) {
		    if ((lp = fopen(stdlog, "r"))) {
			while (fgets(buf, sizeof(buf) -1, lp)) {
			    Striplf(buf);
			    Syslog('+', "stdout: \"%s\"", printable(buf, 0));
			}
			fclose(lp);
		    }
		}
		if (file_size(errlog)) {
		    if ((lp = fopen(errlog, "r"))) {
			while (fgets(buf, sizeof(buf) -1, lp)) {
			    Striplf(buf);
			    Syslog('+', "stderr: \"%s\"", printable(buf, 0));
			}
			fclose(lp);
		    }
		}
		unlink(stdlog);
		unlink(errlog);
		if (err != virscan.error) {
		    WriteError("VIRUS ALERT: Result %d (%s)", err, virscan.comment);
		    colour(CFG.HiliteF, CFG.HiliteB);
		    /* Possible VIRUS found! */
		    printf("%s\n", (char *) Language(199));
		    Found = TRUE;
		} else {
		    /* Ok */
		    printf("%s\n", (char *) Language(200));
		}
		fflush(stdout);
		Altime(0);
		Nopper();
	    }
	}
	fclose(fp);
    }

    execute_pth((char *)"rm", (char *)"-r -f ./*", (char *)"/dev/null", (char *)"/dev/null", (char *)"/dev/null");
    chdir(cwd);
    free(cwd);
    free(temp);
    free(stdlog);
    free(errlog);
	
    if (Found)
	return 2;
    else
	return 0;
}



/*
 * Try to find out the type of uploaded file.
 */
char *GetFileType(char *fn)
{
	unsigned char	buf[8], dbuf[80];
	FILE		*fp;
	int		i;

	if ((fp = fopen(fn, "r")) == NULL) {
		WriteError("$Can't open file %s", fn);
		return NULL;
	}

	if (fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
		WriteError("$Can't read head of file %s", fn);
		return NULL;
	}

	fclose(fp);
	dbuf[0] = '\0';

	for (i = 0; i < sizeof(buf); i++)
		if ((buf[i] >= ' ') && (buf[i] <= 127))
			sprintf((char*)dbuf+strlen(dbuf), "  %c", buf[i]);
		else
			sprintf((char*)dbuf+strlen(dbuf), " %02x", buf[i]);

	/*
	 * Various expected uploads. Not that the standard MS-DOS archivers
	 * must return the exact format, ie "ZIP" for PKZIP. These strings
	 * are tested against the archivers database. Others that aren't
	 * compressed files are not important, they just pop up in your
	 * logfiles.
	 */
 	if (memcmp(buf, "PK\003\004", 4) == 0)		return (char *)"ZIP";
	if (*buf == 0x1a)				return (char *)"ARC";
	if (memcmp(buf+2, "-l", 2) == 0)		return (char *)"LHA";
	if (memcmp(buf, "ZOO", 3) == 0)			return (char *)"ZOO";
	if (memcmp(buf, "`\352", 2) == 0)		return (char *)"ARJ";
	if (memcmp(buf, "Rar!", 4) == 0)		return (char *)"RAR";
	if (memcmp(buf, "HA", 2) == 0)			return (char *)"HA";
	if (memcmp(buf, "MZ", 2) == 0)			return (char *)"EXE";
	if (memcmp(buf, "\000\000\001\263", 4) == 0)	return (char *)"MPEG";
	if (memcmp(buf, "MOVI", 4) == 0)		return (char *)"MOVI";
	if (memcmp(buf, "\007\007\007", 3) == 0)	return (char *)"CPIO";
	if (memcmp(buf, "\351,\001JAM", 6) == 0)	return (char *)"JAM";
	if (memcmp(buf, "SQSH", 4) == 0)		return (char *)"SQSH";
	if (memcmp(buf, "UC2\0x1a", 4) == 0)		return (char *)"UC2";
	if (memcmp(buf, ".snd", 4) == 0)		return (char *)"SND";
	if (memcmp(buf, "MThd", 4) == 0)		return (char *)"MID";
	if (memcmp(buf, "RIFF", 4) == 0)		return (char *)"WAV";
	if (memcmp(buf, "EMOD", 4) == 0)		return (char *)"MOD";
	if (memcmp(buf, "MTM", 3) == 0)			return (char *)"MTM";
	if (memcmp(buf, "#!/bin/", 7) == 0)		return (char *)"UNIX script";
	if (memcmp(buf, "\037\235", 2) == 0)		return (char *)"Compressed data";
	if (memcmp(buf, "\037\213", 2) == 0)		return (char *)"GZIP";
	if (memcmp(buf, "\177ELF", 4) == 0)		return (char *)"ELF";
	if (memcmp(buf, "%!", 2) == 0)			return (char *)"PostScript";
	if (memcmp(buf, "GIF8", 4) == 0)		return (char *)"GIF";
	if (memcmp(buf, "\377\330\377\340", 4) == 0)	return (char *)"JPEG";
	if (memcmp(buf, "\377\330\377\356", 4) == 0)	return (char *)"JPG";
	if (memcmp(buf, "BM", 2) == 0)			return (char *)"Bitmap";
	if (memcmp(buf, "%PDF", 4) == 0)		return (char *)"PDF";
	if (memcmp(buf, "THNL", 4) == 0)		return (char *)"ThumbNail";
	if ((memcmp(buf, "<html>", 6) == 0) ||
	    (memcmp(buf, "<HTML>", 6) == 0))		return (char *)"HTML";
	if (memcmp(buf, "MSCF", 4) == 0)		return (char *)"CAB";
	if (memcmp(buf, "BZ", 2) == 0)			return (char *)"BZIP";

	/*
	 * .COM formats. Should cover about 2/3 of COM files.
	 */
	if ((*buf == 0xe9) || (*buf == 0x8c) ||
	    (*buf == 0xeb) || (*buf == 0xb8))		return (char *)"COM";
	
	return NULL;
}



/*
 * Import file in area. Returns TRUE if successfull.
 */
int ImportFile(char *fn, int Area, int fileid, time_t iTime, off_t Size)
{
    char    *temp, *temp1;

    temp  = calloc(PATH_MAX, sizeof(char));
    temp1 = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/%s", area.Path, fn);
    sprintf(temp1, "%s/%s/upl/%s", CFG.bbs_usersdir, exitinfo.Name, fn);

    if ((file_mv(temp1, temp))) {
	WriteError("$Can't move %s to %s", fn, area.Path);
    } else {
	chmod(temp, 0664);
	if (Addfile(fn, Area, fileid)) {

	    ReadExitinfo();

	    /*
	     * If Size is equal to Zero, don't increase file counters else
	     * Increase file counters if any other size
	     */
	    if (Size) {
		exitinfo.Uploads++;
		exitinfo.UploadK += (Size / 1024);
		exitinfo.UploadKToday += (Size / 1024);
		Syslog('b', "Uploads %d, Kb %d, Kb today %d", exitinfo.Uploads, exitinfo.UploadK, exitinfo.UploadKToday);
		/* You have */  /* extra download KBytes. */
		printf("%s %ld %s\n", (char *) Language(249), (long)(Size / 1024), (char *) Language(250));
	
		exitinfo.DownloadKToday += (Size / 1024);
		Syslog('b', "DownloadKToday %d", exitinfo.DownloadKToday);
	    }

	    iTime /= 60; /* Divide Seconds by 60 to give minutes */
	    /* You have */ /* extra minutes. */
	    printf("%s %ld %s\n", (char *) Language(249), iTime, (char *) Language(259));
	    exitinfo.iTimeLeft += iTime;

	    WriteExitinfo();
	    free(temp);
	    free(temp1);
	    return TRUE;
	}
    }

    free(temp);
    free(temp1);
    return FALSE;
}



/*
 * Add file to the FileDataBase. If fileid is true, then try to
 * get the filedescription from FILE_ID.DIZ if it is in the
 * archive, else the user must supply the description.
 * Returns TRUE is successfull.
 */
int Addfile(char *File, int AreaNum, int fileid)
{
    FILE    *id, *pPrivate;
    int	    err = 1, iDesc = 1, iPrivate = FALSE, GotId = FALSE, lines, i, j;
    char    *Filename, *temp1, *idname = NULL, *Desc[26], *lname, temp[81]; 
    struct  stat statfile; 
#ifdef	USE_EXPERIMENT
    struct _fdbarea *fdb_area = NULL;
#else
    FILE	    *pFileDB;
#endif

    Filename = calloc(PATH_MAX, sizeof(char));
    temp1    = calloc(PATH_MAX, sizeof(char));  
    lname    = calloc(PATH_MAX, sizeof(char));
	
    sprintf(Filename, "%s/%s", area.Path, File);

#ifdef	USE_EXPERIMENT
    if ((fdb_area = mbsedb_OpenFDB(AreaNum, 30))) {
#else
    if ((pFileDB = OpenFileBase(AreaNum, TRUE)) != NULL) {
#endif
	/*
	 * Do a physical check of file to see if it exists
	 * if it fails it will return a zero which will not
	 * increase his uploads stats
	 */
	if (stat(Filename, &statfile) != 0) {

	    colour(10, 0);
	    /* Upload was unsuccessful for: */
	    printf("\n%s%s\n\n", (char *) Language(284), File);

#ifdef	USE_EXPERIMENT
	    mbsedb_CloseFDB(fdb_area);
#else
	    fclose(pFileDB);
#endif
	    free(Filename);
	    free(temp1);
	    free(lname);
	    return FALSE;
	}

	memset(&fdb, 0, fdbhdr.recsize);
	strcpy(fdb.LName, File);
	strcpy(temp1, File);
	name_mangle(temp1);
	strcpy(fdb.Name, temp1);
	fdb.Size = (long)(statfile.st_size);
	fdb.FileDate = statfile.st_mtime;
	fdb.Crc32 = file_crc(Filename, TRUE);
	strcpy(fdb.Uploader, exitinfo.sUserName);
	fdb.UploadDate = time(NULL);
	if (strcmp(fdb.Name, fdb.LName)) {
	    sprintf(lname, "%s/%s", area.Path, fdb.Name);
	    if (symlink(Filename, lname)) {
		WriteError("$Can't create link %s to %s", lname, Filename);
	    }
	}

	if (area.PwdUP) {
	    colour(9,0);
	    /* Do you want to password protect your upload ? [y/N]: */
	    printf("\n%s", (char *) Language(285));
	    fflush(stdout);

	    if (toupper(Getone()) == Keystroke(285, 0)) {
		colour(10, 0);
		/* REMEMBER: Passwords are "CaSe SeNsITiVe!" */
		printf("\n%s\n", (char *) Language(286));
		colour(14,0);
		/* Password: */
		printf("%s", (char *) Language(8));
		fflush(stdout);
		fflush(stdin);
		GetstrC(fdb.Password, 20);
	    }
	}

	if (fileid && strlen(archiver.iunarc)) {
	    /*
	     * The right unarchiver is still in memory,
	     * get the FILE_ID.DIZ if it exists.
	     */
	    sprintf(temp, "%s/%s", area.Path, File);
	    if ((err = execute_str(archiver.iunarc, temp, (char *)"FILE_ID.DIZ", (char *)"/dev/null", 
					(char *)"/dev/null", (char *)"/dev/null"))) {
		if ((err = execute_str(archiver.iunarc, temp, (char *)"file_id.diz", (char *)"/dev/null",
					    (char *)"/dev/null", (char *)"/dev/null"))) {
		    Syslog('+', "No FILE_ID.DIZ found in %s", File);
		} else {
		    idname = xstrcpy((char *)"file_id.diz");
		}
	    } else {
		idname = xstrcpy((char *)"FILE_ID.DIZ");
	    }
	    if (!err) {
		Syslog('+', "Found %s", idname);
		GotId = TRUE;
	    }
	}

	if (GotId) {
	    lines = 0;
	    if ((id = fopen(idname, "r")) != NULL) {
		/*
		 * Import FILE_ID.DIZ, format to max. 25
		 * lines, 48 chars width.
		 */
		while (((fgets(temp1, PATH_MAX -1, id)) != NULL) && (lines < 25)) {
		    Striplf(temp1);
		    if (strlen(temp1) > 51) {
			/*
			 * Malformed FILE_ID.DIZ
			 */
			GotId = FALSE;
			for (i = 0; i < 25; i++)
			    fdb.Desc[i][0] = '\0';
			lines = 0;
			Syslog('!', "Trashing illegal formatted FILE_ID.DIZ");
			break;
		    }
		    if (strlen(temp1) > 0) {
			j = 0;
			for (i = 0; i < strlen(temp1); i++) {
			    if (isprint(temp1[i])) {
				fdb.Desc[lines][j] = temp1[i];
				j++;
				if (j > 47)
				    break;
			    }
			}

			/*
			 * Remove trailing spaces
			 */
			while (j && isspace(fdb.Desc[lines][j-1]))
			    j--;
			fdb.Desc[lines][j] = '\0';
			lines++;
		    }
		}
	    }
	    fclose(id);
	    unlink(idname);

	    if (GotId) {
		/*
		 * Strip empty FILE_ID.DIZ lines at the end
		 */
		while ((strlen(fdb.Desc[lines-1]) == 0) && (lines)) {
		    fdb.Desc[lines-1][0] = '\0';
		    lines--;
		}
		if (lines) {
		    Syslog('+', "Using %d FILE_ID.DIZ lines for description", lines);
		    colour(CFG.TextColourF, CFG.TextColourB);
		    /* Found FILE_ID.DIZ in */
		    printf("%s %s\n", (char *) Language(257), File);
		    fflush(stdout);
		} else {
		    Syslog('!', "No FILE_ID.DIZ lines left to use");
		    GotId = FALSE;
		}
	    }
	} 
	
	if (!GotId) {
	    /*
	     * Ask the user for a description.
	     */
	    for (i = 0; i < 26; i++)
		*(Desc + i) = (char *) calloc(49, sizeof(char));

	    colour(12,0);
	    /* Please enter description of file */
	    printf("\n%s %s\n\n", (char *) Language(287), File);
	    while (TRUE) {
		colour(10,0);
		printf("%2d> ", iDesc);
		fflush(stdout);
		colour(CFG.InputColourF, CFG.InputColourB);

		GetstrC(*(Desc + iDesc), 47);

		if ((strcmp(*(Desc + iDesc), "")) == 0) 
		    break;

		iDesc++;

		if (iDesc >= 26)
		    break;
	    }

	    for (i = 1; i < iDesc; i++)
		strcpy(fdb.Desc[i - 1], Desc[i]);

	    for (i = 0; i < 26; i++)
		free(Desc[i]);
	}

#ifdef	USE_EXPERIMENT
	mbsedb_InsertFDB(fdb_area, fdb, area.AddAlpha);
	mbsedb_CloseFDB(fdb_area);
#else
	fseek(pFileDB, 0, SEEK_END);
	fwrite(&fdb, fdbhdr.recsize, 1, pFileDB);
	fclose(pFileDB);
#endif

	sprintf(temp, "%s/log/uploads.log", getenv("MBSE_ROOT"));
	if ((pPrivate = fopen(temp, "a+")) == NULL)
	    WriteError("$Can't open %s", temp);
	else {
	    iPrivate = TRUE;
	    fprintf(pPrivate, "****************************************************");
	    fprintf(pPrivate, "\nUser        : %s", fdb.Uploader);
	    fprintf(pPrivate, "\nFile        : %s (%s)", fdb.LName, fdb.Name);
	    fprintf(pPrivate, "\nSize        : %lu", (long)(fdb.Size));
	    fprintf(pPrivate, "\nUpload Date : %s\n\n", StrDateDMY(fdb.UploadDate));
				
	    for (i = 0; i < iDesc - 1; i++)
		fprintf(pPrivate, "%2d: %s\n", i, fdb.Desc[i]);

	    fclose(pPrivate);
	}

	Enter(1);
	/* Your upload time has been returned to you. Thank you for your upload! */
	pout(10, 0, (char *) Language(288));
	Enter(1);
    }

    free(Filename);
    free(temp1);
    return TRUE;
}



/*
 * Set file area number, set global area description and path.
 */
void SetFileArea(unsigned long AreaNum)
{
    FILE    *pArea;
    long    offset;

    memset(&area, 0, sizeof(area));

    if ((pArea = OpenFareas(FALSE)) == NULL)
	return;

    offset = areahdr.hdrsize + ((AreaNum - 1) * areahdr.recsize);
    if (fseek(pArea, offset, 0) != 0) {
	WriteError("$Seek error in fareas.data, area %ld", AreaNum);
	return;
    }

    fread(&area, areahdr.recsize, 1, pArea);
    strcpy(sAreaDesc, area.Name);
    strcpy(sAreaPath, area.Path);
    iAreaNumber = AreaNum;
    fclose(pArea);
}



/*
 * Return size in bytes of all files in the users wrk directory.
 */
unsigned long Quota()
{
    DIR		    *dirp;
    char	    *FileName, *temp;
    unsigned long   Bytes = 0;
    struct dirent   *dp;
    struct stat	    statfile;

    FileName = calloc(PATH_MAX, sizeof(char));
    temp     = calloc(PATH_MAX, sizeof(char));

    sprintf(temp, "%s/%s/wrk", CFG.bbs_usersdir, exitinfo.Name);

    if ((dirp = opendir(temp)) == NULL) {
	WriteError("$Can't open dir %s", temp);
    } else {
	while ((dp = readdir(dirp)) != NULL) {
	    sprintf(FileName, "%s/%s", temp, dp->d_name);

	    if (*(dp->d_name) != '.')
		if (stat(FileName, &statfile) == 0)
		    Bytes += statfile.st_size;
	}

	closedir(dirp);
    }

    free(FileName);
    free(temp);
    return Bytes;
}



void ImportHome(char *fn)
{
    char    *temp1, *temp2;

    temp1 = calloc(PATH_MAX, sizeof(char));
    temp2 = calloc(PATH_MAX, sizeof(char));
    sprintf(temp1, "%s/%s/wrk/%s", CFG.bbs_usersdir, exitinfo.Name, fn);
    sprintf(temp2, "%s/%s/upl/%s", CFG.bbs_usersdir, exitinfo.Name, fn);

    Syslog('+', "Move %s to home, result %d", fn, file_mv(temp2, temp1));
    free(temp1);
    free(temp2);
}


