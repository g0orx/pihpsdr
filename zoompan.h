/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
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

#ifndef _ZOOMPAN_H
#define _ZOOMPAN_H

#define MAX_ZOOM 8

extern GtkWidget *zoompan_init(int my_width, int my_height);
extern int zoompan_active_receiver_changed(void *data);

extern void update_pan(double pan);
extern void update_zoom(double zoom);

extern void set_pan(int rx,double value);
extern void set_zoom(int rx,double value);

extern void remote_set_pan(int rx,double value);
extern void remote_set_zoom(int rx,double value);
#endif
