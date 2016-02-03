/**
* @file mode.h
* @brief Header files for the mode functions
* @author John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
* @version 0.1
* @date 2009-04-12
*/
// mode.h

/* Copyright (C)
* 2009 - John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#define modeLSB 0
#define modeUSB 1
#define modeDSB 2
#define modeCWL 3
#define modeCWU 4
#define modeFMN 5
#define modeAM 6
#define modeDIGU 7
#define modeSPEC 8
#define modeDIGL 9
#define modeSAM 10
#define modeDRM 11

#define MODES 12

int mode;

char *mode_string[MODES];

/*
int updateMode(void * data);

void setMode(int mode);
void modeSaveState();
void modeRestoreState();
GtkWidget* buildModeUI();
char* modeToString();
*/
