/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Screen functions for setup. 
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
 * MB BBS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MB BBS; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "../lib/libs.h"
#include "../lib/structs.h"
#include "../lib/ansi.h"
#include "../lib/common.h"
#include "screen.h"


extern int  init;


/*************************************************************************
 *
 *  Global section
 */


void clrtoeol()
{
	int	i;

	printf("\r");
	for (i = 0; i < COLS; i++)
		putchar(' ');
	printf("\r");
	fflush(stdout);
}



void hor_lin(int y, int x, int len)
{
	int	i;

	locate(y, x);
	for (i = 0; i < len; i++)
		putchar('-');
	fflush(stdout);
}



static int	old_f = -1;
static int	old_b = -1;

void set_color(int f, int b)
{
    f = f & 15;
    b = b & 7;

    if ((f != old_f) || (b != old_b)) {
	old_f = f;
	old_b = b;
	colour(f, b);
	fflush(stdout);
    }
}



static time_t lasttime;

/*
 * Show the current date & time in the second status row
 */
void show_date(int fg, int bg, int y, int x)
{
	time_t	now;
	char	*p;

	now = time(NULL);
	if (now != lasttime) {
		lasttime = now;
		set_color(LIGHTGREEN, BLUE);
		p = ctime(&now);
		Striplf(p);
		mvprintw(1, 44, (char *)"%s TZUTC %s", p, gmtoffset(now)); 
		p = asctime(gmtime(&now));
		Striplf(p);
		mvprintw(2, 44, (char *)"%s UTC", p);
		if (y && x)
			locate(y, x);
		set_color(fg, bg);
	}
}



void center_addstr(int y, char *s)
{
	mvprintw(y, (COLS / 2) - (strlen(s) / 2), s);
}



/*
 * Curses and screen initialisation.
 */
void screen_start(char *name)
{
	int	i;

	TermInit(1);
	/*
	 *  Overwrite screen the first time, if user had it black on white
	 *  it will change to white on black. clear() won't do the trick.
	 */
	set_color(LIGHTGRAY, BLUE);
	locate(1, 1);
	for (i = 0; i < LINES; i++) {
		if (i == 3)
			colour(LIGHTGRAY, BLACK);
		clrtoeol();
		if (i < LINES)
			printf("\n");
	}
	fflush(stdout);

	set_color(WHITE, BLUE);
	locate(1, 1);
	printf((char *)"%s for MBSE BBS version %s", name, VERSION);  
	set_color(YELLOW, BLUE);
	locate(2, 1);
	printf((char *)SHORTRIGHT);
	set_color(LIGHTGRAY, BLACK);
	show_date(LIGHTGRAY, BLACK, 0, 0);
	fflush(stdout);
}



/*
 * Screen deinit
 */
void screen_stop()
{
	set_color(LIGHTGRAY, BLACK);
	clear();
	fflush(stdout);
}



/*
 * Message at the upperright window about actions 
 */
void working(int txno, int y, int x)
{
	int	i;

	if (init)
	    return;

	/*
	 * If txno not 0 there will be something written. The 
	 * reversed attributes for mono, or white on red for
	 * color screens is set. The cursor is turned off and 
	 * original cursor position is saved.
	 */
	show_date(LIGHTGRAY, BLACK, 0, 0);

	if (txno != 0)
		set_color(YELLOW, RED);
	else
		set_color(LIGHTGRAY, BLACK);

	switch (txno) {
	case 0: mvprintw(4, 66, (char *)"             ");
		break;
	case 1: mvprintw(4, 66, (char *)"Working . . .");
		break;
	case 2:	mvprintw(4, 66, (char *)">>> ERROR <<<");
		for (i = 1; i <= 5; i++) {
			putchar(7);
			fflush(stdout);
			usleep(150000);
		}
		usleep(550000);
		break;
	case 3: mvprintw(4, 66, (char *)"Form inserted");
		putchar(7);
		fflush(stdout);
		sleep(1);
		break;
	case 4: mvprintw(4, 66, (char *)"Form deleted ");
		putchar(7);
		fflush(stdout);
		sleep(1);
		break;
	case 5: mvprintw(4, 66, (char *)"Moving . . . ");
		break;
	}

	show_date(LIGHTGRAY, BLACK, 0, 0);
	set_color(LIGHTGRAY, BLACK);
	if (y && x)
		locate(y, x);
	fflush(stdout);
}



/*
 *  Clear the middle window 
 */
void clr_index()
{
	int i;

	set_color(LIGHTGRAY, BLACK);
	for (i = 3; i <= (LINES - 1); i++) {
		locate(i, 1);
		clrtoeol();
	}
}



/*
 * Show help at the bottom of the screen.
 */
void showhelp(char *T)
{
	int f, i, x, forlim;

	f = FALSE;
	locate(LINES-1, 1);
	set_color(WHITE, RED);
	clrtoeol();
	x = 0;
	forlim = strlen(T);

	for (i = 0; i < forlim; i++) {
		if (T[i] == '^') {
			if (f == FALSE) {
				f = TRUE;
				set_color(YELLOW, RED);
			} else {
				f = FALSE;
				set_color(WHITE, RED);
			}
		} else {
			putchar(T[i]);
			x++;
		}
	}
	set_color(LIGHTGRAY, BLACK);
	fflush(stdout);
}


