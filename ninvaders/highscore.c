/**
 * nInvaders - a space invaders clone for ncurses
 * Copyright (C) 2002-2003 Dettus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * homepage: http://ninvaders.sourceforge.net
 * mailto: ninvaders-devel@lists.sourceforge.net
 *
 */

 
#include "highscore.h"
#include <stdio.h>	
#include <string.h>


/**************************************************************************************************
 * 
 * private constants, variables, structures, declarations, macros
 *
 **************************************************************************************************/

#define HIGHSCORE_FILE          "highscore"		   /* filename of highscore             */
#define HIGHSCORE_ID            "nInvaders Highscore"      /* header for highscore file         */
#define HIGHSCORE_VERSION       "0.2"                      /* version number for highscore file */



/**************************************************************************************************
 *
 * getStandardHighScore
 *
 * returns a HighScore object initialized with standard values
 *
 **************************************************************************************************/
HighScore getStandardHighScore()
{
	HighScore hs;
	int n;

	for (n = 0; n < MAX_HIGHSCORE_ENTRIES; n++) {
		hs.entries[n].score = (MAX_HIGHSCORE_ENTRIES - n) * 1000;
		strcpy (hs.entries[n].name, "sen_hoss");
	}

	return hs;
}



/**************************************************************************************************
 *
 * writeHighScore
 *
 * writes highscore to file 
 * return value
 *     1 if successful
 *     0 if error occurred
 *
 **************************************************************************************************/
int writeHighScore (HighScore hs)
{
//	FILE* fp_HighScore;
//	int n;

//	if (( fp_HighScore = fopen (HIGHSCORE_FILE, "w") ) == NULL ) {
//		puts ("(EE) writeHighScore: cannot open highscore file for writing");
//		return 0;
//	}

	/* write header */
//	fprintf (fp_HighScore, "%s\nv%s\n\n", HIGHSCORE_ID, HIGHSCORE_VERSION);

	/* write data */
//	for (n = 0; n < MAX_HIGHSCORE_ENTRIES; n++) {
//		fprintf (fp_HighScore, "%i %s\n", hs.entries[n].score, hs.entries[n].name);
//	}

//	fclose (fp_HighScore);
	return 1;
}



/**************************************************************************************************
 *
 * readHighScore
 *
 * reads highscore from file; falls back to the standard table if the file
 * is missing, has the wrong header/version, or is incomplete
 *
 **************************************************************************************************/
HighScore readHighScore ()
{
//	FILE* fp_HighScore;
//	HighScore hs;
//	char hs_id      [ sizeof(HIGHSCORE_ID) ];
//	char hs_version [ sizeof(HIGHSCORE_VERSION) ];
//	int n;

	HighScore standard = getStandardHighScore();

//	if (( fp_HighScore = fopen (HIGHSCORE_FILE, "r") ) == NULL ) {
//		puts ("(EE) readHighScore: cannot open highscore file for reading");
//		return standard;
//	}

	/* read header */
//	if (fscanf (fp_HighScore, "%19[^\n]\nv%3[^\n]\n\n", hs_id, hs_version) != 2
//	    || strcmp (hs_id, HIGHSCORE_ID) != 0
//	    || strcmp (hs_version, HIGHSCORE_VERSION) != 0) {
//		puts ("(EE) readHighScore: highscore file has wrong header or version");
//		fclose (fp_HighScore);
//		return standard;
//	}

	/* read data */
//	for (n = 0; n < MAX_HIGHSCORE_ENTRIES; n++) {
//		if (fscanf (fp_HighScore, "%i %s\n", &hs.entries[n].score, hs.entries[n].name) != 2) {
//			puts ("(EE) readHighScore: highscore file is incomplete");
//			fclose (fp_HighScore);
//			return standard;
//		}
//	}

//	fclose (fp_HighScore);
	return standard;
}



/**************************************************************************************************
 *
 * addEntry
 *
 * inserts a score into the (sorted) highscore table
 *
 **************************************************************************************************/
void addEntry(char *name, int score)
{
	int i = 0;
	int j;

	while (highscore.entries[i].score >= score) {
		i++;
	}

	for (j = MAX_HIGHSCORE_ENTRIES; j > i; j--) {
		highscore.entries[j] = highscore.entries[j - 1];
	}

	highscore.entries[i].score = score;
	strcpy(highscore.entries[i].name, name);
}
