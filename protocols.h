/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
*
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

extern gboolean enable_protocol_1;
extern gboolean enable_protocol_2;
#ifdef SOAPYSDR
extern gboolean enable_soapy_protocol;
#endif
#ifdef STEMLAB_DISCOVERY
extern gboolean enable_stemlab;
#endif
extern gboolean autostart

extern void protocols_save_state();
extern void protocols_restore_state();
extern void configure_protocols(GtkWidget *parent);
