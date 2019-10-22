/** 
* @file frequency.c
* @brief Frequency functions
* @author John Melton, G0ORX/N6LYT, Doxygen Comments Dave Larsen, KV0S
* @version 0.1
* @date 2009-04-11
*/

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


//
// frequency.c
//

#include <gtk/gtk.h>
#include "band.h"
#include "frequency.h"

char* outOfBand="Out of band";
struct frequency_info* info;

//
// For the ease of maintenance, keep this Table aligned from the top
// through the bottom
//
#define NOBAND -1
struct frequency_info frequencyInfo[]=
    {

        {      60000LL,       60000LL, "MSF Time Signal",                    bandGen, FALSE}, 
        {      75000LL,       75000LL, "HGB Time Signal",                    bandGen, FALSE}, 
        {      77500LL,       77500LL, "DCF77 Time Signal",                  bandGen, FALSE}, 
        {     135700LL,      137800LL, "136kHz CW",                          band136, TRUE },
        {     472000LL,      474999LL, "472kHz CW",                          band472, TRUE },
        {     475000LL,      479000LL, "472kHz CW/Data",                     band472, TRUE },
        {     153000LL,      279000LL, "AM - Long Wave",                     bandGen, FALSE}, 
        {     530000LL,     1710000LL, "Broadcast AM Med Wave",              bandGen, FALSE},                                 

        {    1800000LL,     1809999LL, "160M CW/Digital Modes",              band160, TRUE },
        {    1810000LL,     1810000LL, "160M CW QRP",                        band160, TRUE },
        {    1810001LL,     1842999LL, "160M CW",                            band160, TRUE },
        {    1843000LL,     1909999LL, "160M SSB/SSTV/Wide Band",            band160, TRUE },
        {    1910000LL,     1910000LL, "160M SSB QRP",                       band160, TRUE },
        {    1910001LL,     1994999LL, "160M SSB/SSTV/Wide Band",            band160, TRUE },
        {    1995000LL,     1999999LL, "160M Experimental",                  band160, TRUE },

        {    2300000LL,     2495000LL, "120M Short Wave",                    bandGen, FALSE}, 

        {    2500000LL,     2500000LL, "WWV",                                bandWWV, FALSE},

        {    3200000LL,     3400000LL, "90M Short Wave",                     bandGen, FALSE}, 

        {    3500000LL,     3524999LL, "80M Extra CW",                       band80,  TRUE },
        {    3525000LL,     3579999LL, "80M CW",                             band80,  TRUE },
        {    3580000LL,     3589999LL, "80M RTTY",                           band80,  TRUE },
        {    3590000LL,     3590000LL, "80M RTTY DX",                        band80,  TRUE },
        {    3590001LL,     3599999LL, "80M RTTY",                           band80,  TRUE },
        {    3600000LL,     3699999LL, "75M Extra SSB",                      band80,  TRUE },
        {    3700000LL,     3719999LL, "75M Ext/Adv SSB",                    band80,  TRUE },
        {    3720000LL,     3723999LL, "75M Digital Voice",                  band80,  TRUE },
        {    3724000LL,     3789999LL, "75M Ext/Adv SSB",                    band80,  TRUE },
        {    3790000LL,     3799999LL, "75M Ext/Adv DX Window",              band80,  TRUE },
        {    3800000LL,     3844999LL, "75M SSB",                            band80,  TRUE },
        {    3845000LL,     3845000LL, "75M SSTV",                           band80,  TRUE },
        {    3845001LL,     3884999LL, "75M SSB",                            band80,  TRUE },
        {    3885000LL,     3885000LL, "75M AM Calling Frequency",           band80,  TRUE },
        {    3885001LL,     3999999LL, "75M SSB",                            band80,  TRUE },

        {    4750000LL,     4999999LL, "60M Short Wave",                     bandGen, FALSE}, 

        {    5000000LL,     5000000LL, "WWV",                                bandWWV, FALSE}, 

        {    5261250LL,     5408000LL, "60M",                                band60,  TRUE },
        {    5261250LL,     5408000LL, "60M",                                band60,  FALSE},

        {    5900000LL,     6200000LL, "49M Short Wave",                     bandGen, FALSE}, 

        {    7000000LL,     7024999LL, "40M Extra CW",                       band40,  TRUE },
        {    7025000LL,     7039999LL, "40M CW",                             band40,  TRUE },
        {    7040000LL,     7040000LL, "40M RTTY DX",                        band40,  TRUE },
        {    7040001LL,     7099999LL, "40M RTTY",                           band40,  TRUE },
        {    7100000LL,     7124999LL, "40M CW",                             band40,  TRUE },
        {    7125000LL,     7170999LL, "40M Ext/Adv SSB",                    band40,  TRUE },
        {    7171000LL,     7171000LL, "40M SSTV",                           band40,  TRUE },
        {    7171001LL,     7174999LL, "40M Ext/Adv SSB",                    band40,  TRUE },
        {    7175000LL,     7289999LL, "40M SSB",                            band40,  TRUE },
        {    7290000LL,     7290000LL, "40M AM Calling Frequency",           band40,  TRUE },
        {    7290001LL,     7299999LL, "40M SSB",                            band40,  TRUE },

        {    7300000LL,     7350000LL, "41M Short Wave",                     bandGen, FALSE}, 
        {    9400000LL,     9900000LL, "31M Short Wave",                     bandGen, FALSE}, 

        {   10000000LL,    10000000LL, "WWV",                                bandWWV, FALSE}, 

        {   10100000LL,    10129999LL, "30M CW",                             band30,  TRUE },
        {   10130000LL,    10139999LL, "30M RTTY",                           band30,  TRUE },
        {   10140000LL,    10149999LL, "30M Packet",                         band30,  TRUE },

        {   11600000LL,    12100000LL, "25M Short Wave",                     bandGen, FALSE}, 
        {   13570000LL,    13870000LL, "22M Short Wave",                     bandGen, FALSE}, 

        {   14000000LL,    14024999LL, "20M Extra CW",                       band20,  TRUE },
        {   14025000LL,    14069999LL, "20M CW",                             band20,  TRUE },
        {   14070000LL,    14094999LL, "20M RTTY",                           band20,  TRUE },
        {   14095000LL,    14099499LL, "20M Packet",                         band20,  TRUE },
        {   14099500LL,    14099999LL, "20M CW",                             band20,  TRUE },
        {   14100000LL,    14100000LL, "20M NCDXF Beacons",                  band20,  TRUE },
        {   14100001LL,    14100499LL, "20M CW",                             band20,  TRUE },
        {   14100500LL,    14111999LL, "20M Packet",                         band20,  TRUE },
        {   14112000LL,    14149999LL, "20M CW",                             band20,  TRUE },
        {   14150000LL,    14174999LL, "20M Extra SSB",                      band20,  TRUE },
        {   14175000LL,    14224999LL, "20M Ext/Adv SSB",                    band20,  TRUE },
        {   14225000LL,    14229999LL, "20M SSB",                            band20,  TRUE },
        {   14230000LL,    14235999LL, "20M SSTV",                           band20,  TRUE },
        {   14236000LL,    14239999LL, "20M Digital Voice",                  band20,  TRUE },
        {   14240000LL,    14284999LL, "20M SSB",                            band20,  TRUE },
        {   14285000LL,    14285000LL, "20M SSB QRP Calling Frequency",      band20,  TRUE },
        {   14285000LL,    14285999LL, "20M SSB",                            band20,  TRUE },
        {   14286000LL,    14286000LL, "20M AM Calling Frequency",           band20,  TRUE },
        {   14286001LL,    14349999LL, "20M SSB",                            band20,  TRUE },

        {   15000000LL,    15000000LL, "WWV",                                bandWWV, FALSE}, 

        {   15100000LL,    15800000LL, "19M Short Wave",                     bandGen, FALSE}, 
        {   17480000LL,    17900000LL, "16M Short Wave",                     bandGen, FALSE}, 

        {   18068000LL,    18099999LL, "17M CW",                             band17,  TRUE },
        {   18100000LL,    18104999LL, "17M RTTY",                           band17,  TRUE },
        {   18105000LL,    18109999LL, "17M Packet",                         band17,  TRUE },
        {   18110000LL,    18110000LL, "17M NCDXF Beacons",                  band17,  TRUE },
        {   18110001LL,    18167999LL, "17M SSB",                            band17,  TRUE },

        {   18900000LL,    19020000LL, "15M Short Wave",                     bandGen, FALSE}, 

        {   20000000LL,    20000000LL, "WWV",                                bandWWV, FALSE}, 

        {   21000000LL,    21024999LL, "15M Extra CW",                       band15,  TRUE },
        {   21025000LL,    21069999LL, "15M CW",                             band15,  TRUE },
        {   21070000LL,    21099999LL, "15M RTTY",                           band15,  TRUE },
        {   21100000LL,    21109999LL, "15M Packet",                         band15,  TRUE },
        {   21110000LL,    21149999LL, "15M CW",                             band15,  TRUE },
        {   21150000LL,    21150000LL, "15M NCDXF Beacons",                  band15,  TRUE },
        {   21150001LL,    21199999LL, "15M CW",                             band15,  TRUE },
        {   21200000LL,    21224999LL, "15M Extra SSB",                      band15,  TRUE },
        {   21225000LL,    21274999LL, "15M Ext/Adv SSB",                    band15,  TRUE },
        {   21275000LL,    21339999LL, "15M SSB",                            band15,  TRUE },
        {   21340000LL,    21340000LL, "15M SSTV",                           band15,  TRUE },
        {   21340001LL,    21449999LL, "15M SSB",                            band15,  TRUE },

        {   21450000LL,    21850000LL, "13M Short Wave",                     bandGen, FALSE}, 

        {   24890000LL,    24919999LL, "12M CW",                             band12,  TRUE },
        {   24920000LL,    24924999LL, "12M RTTY",                           band12,  TRUE },
        {   24925000LL,    24929999LL, "12M Packet",                         band12,  TRUE },
        {   24930000LL,    24930000LL, "12M NCDXF Beacons",                  band12,  TRUE },
        {   24930001LL,    24989999LL, "12M SSB Wideband",                   band12,  TRUE },

        {   25600000LL,    26100000LL, "11M Short Wave",                     bandGen, FALSE}, 

        {   28000000LL,    28069999LL, "10M CW",                             band10,  TRUE },
        {   28070000LL,    28149999LL, "10M RTTY",                           band10,  TRUE },
        {   28150000LL,    28199999LL, "10M CW",                             band10,  TRUE },
        {   28200000LL,    28200000LL, "10M NCDXF Beacons",                  band10,  TRUE },
        {   28200001LL,    28299999LL, "10M Beacons",                        band10,  TRUE },
        {   28300000LL,    28679999LL, "10M SSB",                            band10,  TRUE },
        {   28680000LL,    28680000LL, "10M SSTV",                           band10,  TRUE },
        {   28680001LL,    28999999LL, "10M SSB",                            band10,  TRUE },
        {   29000000LL,    29199999LL, "10M AM",                             band10,  TRUE },
        {   29200000LL,    29299999LL, "10M SSB",                            band10,  TRUE },
        {   29300000LL,    29509999LL, "10M Satellite Downlinks",            band10,  TRUE },
        {   29510000LL,    29519999LL, "10M Deadband",                       band10,  TRUE },
        {   29520000LL,    29589999LL, "10M Repeater Inputs",                band10,  TRUE },
        {   29590000LL,    29599999LL, "10M Deadband",                       band10,  TRUE },
        {   29600000LL,    29600000LL, "10M FM Simplex",                     band10,  TRUE },
        {   29600001LL,    29609999LL, "10M Deadband",                       band10,  TRUE },
        {   29610000LL,    29699999LL, "10M Repeater Outputs",               band10,  TRUE },

        {   50000000LL,    50059999LL, "6M CW",                              band6,   TRUE },
        {   50060000LL,    50079999LL, "6M Beacon Sub-Band",                 band6,   TRUE },
        {   50080000LL,    50099999LL, "6M CW",                              band6,   TRUE },
        {   50100000LL,    50124999LL, "6M DX Window",                       band6,   TRUE },
        {   50125000LL,    50125000LL, "6M Calling Frequency",               band6,   TRUE },
        {   50125001LL,    50299999LL, "6M SSB",                             band6,   TRUE },
        {   50300000LL,    50599999LL, "6M All Modes",                       band6,   TRUE },
        {   50600000LL,    50619999LL, "6M Non Voice",                       band6,   TRUE },
        {   50620000LL,    50620000LL, "6M Digital Packet Calling",          band6,   TRUE },
        {   50620001LL,    50799999LL, "6M Non Voice",                       band6,   TRUE },
        {   50800000LL,    50999999LL, "6M RC",                              band6,   TRUE },
        {   51000000LL,    51099999LL, "6M Pacific DX Window",               band6,   TRUE },
        {   51100000LL,    51119999LL, "6M Deadband",                        band6,   TRUE },
        {   51120000LL,    51179999LL, "6M Digital Repeater Inputs",         band6,   TRUE },
        {   51180000LL,    51479999LL, "6M Repeater Inputs",                 band6,   TRUE },
        {   51480000LL,    51619999LL, "6M Deadband",                        band6,   TRUE },
        {   51620000LL,    51679999LL, "6M Digital Repeater Outputs",        band6,   TRUE },
        {   51680000LL,    51979999LL, "6M Repeater Outputs",                band6,   TRUE },
        {   51980000LL,    51999999LL, "6M Deadband",                        band6,   TRUE },
        {   52000000LL,    52019999LL, "6M Repeater Inputs",                 band6,   TRUE },
        {   52020000LL,    52020000LL, "6M FM Simplex",                      band6,   TRUE },
        {   52020001LL,    52039999LL, "6M Repeater Inputs",                 band6,   TRUE },
        {   52040000LL,    52040000LL, "6M FM Simplex",                      band6,   TRUE },
        {   52040001LL,    52479999LL, "6M Repeater Inputs",                 band6,   TRUE },
        {   52480000LL,    52499999LL, "6M Deadband",                        band6,   TRUE },
        {   52500000LL,    52524999LL, "6M Repeater Outputs",                band6,   TRUE },
        {   52525000LL,    52525000LL, "6M Primary FM Simplex",              band6,   TRUE },
        {   52525001LL,    52539999LL, "6M Deadband",                        band6,   TRUE },
        {   52540000LL,    52540000LL, "6M Secondary FM Simplex",            band6,   TRUE },
        {   52540001LL,    52979999LL, "6M Repeater Outputs",                band6,   TRUE },
        {   52980000LL,    52999999LL, "6M Deadbands",                       band6,   TRUE },
        {   53000000LL,    53000000LL, "6M Remote Base FM Spx",              band6,   TRUE },
        {   53000001LL,    53019999LL, "6M Repeater Inputs",                 band6,   TRUE },
        {   53020000LL,    53020000LL, "6M FM Simplex",                      band6,   TRUE },
        {   53020001LL,    53479999LL, "6M Repeater Inputs",                 band6,   TRUE },
        {   53480000LL,    53499999LL, "6M Deadband",                        band6,   TRUE },
        {   53500000LL,    53519999LL, "6M Repeater Outputs",                band6,   TRUE },
        {   53520000LL,    53520000LL, "6M FM Simplex",                      band6,   TRUE },
        {   53520001LL,    53899999LL, "6M Repeater Outputs",                band6,   TRUE },
        {   53900000LL,    53900000LL, "6M FM Simplex",                      band6,   TRUE },
        {   53900010LL,    53979999LL, "6M Repeater Outputs",                band6,   TRUE },
        {   53980000LL,    53999999LL, "6M Deadband",                        band6,   TRUE },

        {  144000000LL,   144099999LL, "2M CW",                              NOBAND,  TRUE },
        {  144100000LL,   144199999LL, "2M CW/SSB",                          NOBAND,  TRUE },
        {  144200000LL,   144200000LL, "2M Calling",                         NOBAND,  TRUE },
        {  144200001LL,   144274999LL, "2M CW/SSB",                          NOBAND,  TRUE },
        {  144275000LL,   144299999LL, "2M Beacon Sub-Band",                 NOBAND,  TRUE },
        {  144300000LL,   144499999LL, "2M Satellite",                       NOBAND,  TRUE },
        {  144500000LL,   144599999LL, "2M Linear Translator Inputs",        NOBAND,  TRUE },
        {  144600000LL,   144899999LL, "2M FM Repeater",                     NOBAND,  TRUE },
        {  144900000LL,   145199999LL, "2M FM Simplex",                      NOBAND,  TRUE },
        {  145200000LL,   145499999LL, "2M FM Repeater",                     NOBAND,  TRUE },
        {  145500000LL,   145799999LL, "2M FM Simplex",                      NOBAND,  TRUE },
        {  145800000LL,   145999999LL, "2M Satellite",                       NOBAND,  TRUE },
        {  146000000LL,   146399999LL, "2M FM Repeater",                     NOBAND,  TRUE },
        {  146400000LL,   146609999LL, "2M FM Simplex",                      NOBAND,  TRUE },
        {  146610000LL,   147389999LL, "2M FM Repeater",                     NOBAND,  TRUE },
        {  147390000LL,   147599999LL, "2M FM Simplex",                      NOBAND,  TRUE },
        {  147600000LL,   147999999LL, "2M FM Repeater",                     NOBAND,  TRUE },

        {  222000000LL,   222024999LL, "125CM EME/Weak Signal",              NOBAND,  TRUE },
        {  222025000LL,   222049999LL, "125CM Weak Signal",                  NOBAND,  TRUE },
        {  222050000LL,   222059999LL, "125CM Propagation Beacons",          NOBAND,  TRUE },
        {  222060000LL,   222099999LL, "125CM Weak Signal",                  NOBAND,  TRUE },
        {  222100000LL,   222100000LL, "125CM SSB/CW Calling",               NOBAND,  TRUE },
        {  222100001LL,   222149999LL, "125CM Weak Signal CW/SSB",           NOBAND,  TRUE },
        {  222150000LL,   222249999LL, "125CM Local Option",                 NOBAND,  TRUE },
        {  222250000LL,   223380000LL, "125CM FM Repeater Inputs",           NOBAND,  TRUE },
        {  222380001LL,   223399999LL, "125CM General",                      NOBAND,  TRUE },
        {  223400000LL,   223519999LL, "125CM FM Simplex",                   NOBAND,  TRUE },
        {  223520000LL,   223639999LL, "125CM Digital/Packet",               NOBAND,  TRUE },
        {  223640000LL,   223700000LL, "125CM Links/Control",                NOBAND,  TRUE },
        {  223700001LL,   223709999LL, "125CM General",                      NOBAND,  TRUE },
        {  223710000LL,   223849999LL, "125CM Local Option",                 NOBAND,  TRUE },
        {  223850000LL,   224980000LL, "125CM Repeater Outputs",             NOBAND,  TRUE },

        {  420000000LL,   425999999LL, "70CM ATV Repeater",                  NOBAND,  TRUE },
        {  426000000LL,   431999999LL, "70CM ATV Simplex",                   NOBAND,  TRUE },
        {  432000000LL,   432069999LL, "70CM EME",                           NOBAND,  TRUE },
        {  432070000LL,   432099999LL, "70CM Weak Signal CW",                NOBAND,  TRUE },
        {  432100000LL,   432100000LL, "70CM Calling Frequency",             NOBAND,  TRUE },
        {  432100001LL,   432299999LL, "70CM Mixed Mode Weak Signal",        NOBAND,  TRUE },
        {  432300000LL,   432399999LL, "70CM Propagation Beacons",           NOBAND,  TRUE },
        {  432400000LL,   432999999LL, "70CM Mixed Mode Weak Signal",        NOBAND,  TRUE },
        {  433000000LL,   434999999LL, "70CM Auxillary/Repeater Links",      NOBAND,  TRUE },
        {  435000000LL,   437999999LL, "70CM Satellite Only",                NOBAND,  TRUE },
        {  438000000LL,   441999999LL, "70CM ATV Repeater",                  NOBAND,  TRUE },
        {  442000000LL,   444999999LL, "70CM Local Repeaters",               NOBAND,  TRUE },
        {  445000000LL,   445999999LL, "70CM Local Option",                  NOBAND,  TRUE },
        {  446000000LL,   446000000LL, "70CM Simplex",                       NOBAND,  TRUE },
        {  446000001LL,   446999999LL, "70CM Local Option",                  NOBAND,  TRUE },
        {  447000000LL,   450000000LL, "70CM Local Repeaters",               NOBAND,  TRUE },


        {  902000000LL,   902099999LL, "33CM Weak Signal SSTV/FAX/ACSSB",    NOBAND,  TRUE },
        {  902100000LL,   902100000LL, "33CM Weak Signal Calling",           NOBAND,  TRUE },
        {  902100001LL,   902799999LL, "33CM Weak Signal SSTV/FAX/ACSSB",    NOBAND,  TRUE },
        {  902800000LL,   902999999LL, "33CM Weak Signal EME/CW",            NOBAND,  TRUE },
        {  903000000LL,   903099999LL, "33CM Digital Modes",                 NOBAND,  TRUE },
        {  903100000LL,   903100000LL, "33CM Alternate Calling",             NOBAND,  TRUE },
        {  903100001LL,   905999999LL, "33CM Digital Modes",                 NOBAND,  TRUE },
        {  906000000LL,   908999999LL, "33CM FM Repeater Inputs",            NOBAND,  TRUE },
        {  909000000LL,   914999999LL, "33CM ATV",                           NOBAND,  TRUE },
        {  915000000LL,   917999999LL, "33CM Digital Modes",                 NOBAND,  TRUE },
        {  918000000LL,   920999999LL, "33CM FM Repeater Outputs",           NOBAND,  TRUE },
        {  921000000LL,   926999999LL, "33CM ATV",                           NOBAND,  TRUE },
        {  927000000LL,   928000000LL, "33CM FM Simplex/Links",              NOBAND,  TRUE },
         
        { 1240000000LL,  1245999999LL, "23CM ATV #1",                        NOBAND,  TRUE },
        { 1246000000LL,  1251999999LL, "23CM FMN Point/Links",               NOBAND,  TRUE },
        { 1252000000LL,  1257999999LL, "23CM ATV #2, Digital Modes",         NOBAND,  TRUE },
        { 1258000000LL,  1259999999LL, "23CM FMN Point/Links",               NOBAND,  TRUE },
        { 1260000000LL,  1269999999LL, "23CM Sat Uplinks/Wideband Exp",      NOBAND,  TRUE },
        { 1270000000LL,  1275999999LL, "23CM Repeater Inputs",               NOBAND,  TRUE },
        { 1276000000LL,  1281999999LL, "23CM ATV #3",                        NOBAND,  TRUE },
        { 1282000000LL,  1287999999LL, "23CM Repeater Outputs",              NOBAND,  TRUE },
        { 1288000000LL,  1293999999LL, "23CM Simplex ATV/Wideband Exp",      NOBAND,  TRUE },
        { 1294000000LL,  1294499999LL, "23CM Simplex FMN",                   NOBAND,  TRUE },
        { 1294500000LL,  1294500000LL, "23CM FM Simplex Calling",            NOBAND,  TRUE },
        { 1294500001LL,  1294999999LL, "23CM Simplex FMN",                   NOBAND,  TRUE },
        { 1295000000LL,  1295799999LL, "23CM SSTV/FAX/ACSSB/Exp",            NOBAND,  TRUE },
        { 1295800000LL,  1295999999LL, "23CM EME/CW Expansion",              NOBAND,  TRUE },
        { 1296000000LL,  1296049999LL, "23CM EME Exclusive",                 NOBAND,  TRUE },
        { 1296050000LL,  1296069999LL, "23CM Weak Signal",                   NOBAND,  TRUE },
        { 1296070000LL,  1296079999LL, "23CM CW Beacons",                    NOBAND,  TRUE },
        { 1296080000LL,  1296099999LL, "23CM Weak Signal",                   NOBAND,  TRUE },
        { 1296100000LL,  1296100000LL, "23CM CW/SSB Calling",                NOBAND,  TRUE },
        { 1296100001LL,  1296399999LL, "23CM Weak Signal",                   NOBAND,  TRUE },
        { 1296400000LL,  1296599999LL, "23CM X-Band Translator Input",       NOBAND,  TRUE },
        { 1296600000LL,  1296799999LL, "23CM X-Band Translator Output",      NOBAND,  TRUE },
        { 1296800000LL,  1296999999LL, "23CM Experimental Beacons",          NOBAND,  TRUE },
        { 1297000000LL,  1300000000LL, "23CM Digital Modes",                 NOBAND,  TRUE },

        { 2300000000LL,  2449999999LL, "13cm",              NOBAND,  TRUE },

        { 3456000000LL,  3456099999LL, "3.4GHz General",                     NOBAND,  TRUE },
        { 3456100000LL,  3456100000LL, "3.4GHz Calling Frequency",           NOBAND,  TRUE },
        { 3456100001LL,  3456299999LL, "3.4GHz General",                     NOBAND,  TRUE },
        { 3456300000LL,  3456400000LL, "3.4GHz Propagation Beacons",         NOBAND,  TRUE },

        { 5760000000LL,  5760099999LL, "5.7GHz General",                     NOBAND,  TRUE },
        { 5760100000LL,  5760100000LL, "5.7GHz Calling Frequency",           NOBAND,  TRUE },
        { 5760100001LL,  5760299999LL, "5.7GHz General",                     NOBAND,  TRUE },
        { 5760300000LL,  5760400000LL, "5.7GHz Propagation Beacons",         NOBAND,  TRUE },

        {10000000000LL, 10499999999LL, "10GHz",                              NOBAND,  TRUE },

        {24192000000LL, 24192099999LL, "24GHz General",                      NOBAND,  TRUE },
        {24192100000LL, 24192100000LL, "24GHz Calling Frequency",            NOBAND,  TRUE },
        {24192100001LL, 24192400000LL, "24GHz General",                      NOBAND,  TRUE },

        {47088000000LL, 47088099999LL, "47GHz General",                      NOBAND,  TRUE },
        {47088100000LL, 47088100000LL, "47GHz Calling Frequency",            NOBAND,  TRUE },
        {47088100001LL, 47088400000LL, "47GHz General",                      NOBAND,  TRUE },



        {0LL,           0LL,           "",                                   0,       FALSE}

        

    };

/* --------------------------------------------------------------------------*/
/** 
* @brief iGet the frequency information
* 
* @param frequency
* 
* @return 
*/
char* getFrequencyInfo(long long frequency,int filter_low,int filter_high) {

    char* result=outOfBand;

    long long flow=frequency+(long long)filter_low;
    long long fhigh=frequency+(long long)filter_high;

//fprintf(stderr,"getFrequency: frequency=%lld filter_low=%d filter_high=%d flow=%lld fhigh=%lld\n",
//    frequency,filter_low,filter_high,flow,fhigh);

    info=frequencyInfo;

    while(info->minFrequency!=0) {
        if(flow<info->minFrequency) {
            info=0;
            break;
        } else if(flow>=info->minFrequency && fhigh<=info->maxFrequency) {
            if(info->band==band60) {
              int i;
              for(i=0;i<channel_entries;i++) {
                long long low_freq=band_channels_60m[i].frequency-(band_channels_60m[i].width/(long long)2);
                long long hi_freq=band_channels_60m[i].frequency+(band_channels_60m[i].width/(long long)2);
//fprintf(stderr,"channel: %d frequency=%lld width=%lld\n",i,band_channels_60m[i].frequency,band_channels_60m[i].width);
                if(flow>=low_freq && fhigh<=hi_freq) {
                  result=info->info;
                  break;
                }
              }
              if(i>=channel_entries) {
                info++;   // next entry is the whole 60m-Band with transmit = FALSE
              }
              break;
            } else {
              result=info->info;
            }
            break;
        } else if ((info->maxFrequency +1) == ((info+1)->minFrequency) &&
                   flow>=info->minFrequency && fhigh <= (info+1)->maxFrequency) {
            //
	    // Sometimes in DigiMode operation, the nominal USB TX channel crosses
	    // a sub-band boundary. Example: FT8 on 17m, VFO set to 18099 kHz because
	    // FT8 signal is generated at 18100.8 kHz with an AF signal of 1800 Hz.
	    // Here we allow transmit in this case. If we arrive at this point,
	    // we have two adjacent sub-bands, and parts of the TX signal fall into
	    // either sub-band. We can return the info for the lower sub-band if
	    // the band and transmit data is the same in both cases. If they are
	    // different, we have found no match so far.
	    //
	    if ((info->band == (info+1)->band) && (info->transmit == (info+1)->transmit)) {
	  	result = info->info;
		break;
	    }
        }
        info++;
    }

//fprintf(stderr,"info: %s tx=%d\n", info->info, info->transmit);

    return result;
}

/* --------------------------------------------------------------------------*/
/** 
* @brief Get Band information
* 
* @param frequency
* 
* @return 
*/
int getBand(long long frequency) {

    int result=bandGen;

    info=frequencyInfo;

    while(info->minFrequency!=0) {
        if(frequency<info->minFrequency) {
            info=0;
            break;
        } else if(frequency>=info->minFrequency && frequency<=info->maxFrequency) {
            result=info->band;
            break;
        }
        info++;
    }

    return result;
}

int canTransmit() {
    int result=0;
    if(info!=0) {
        result=info->transmit;
    }
    return result;
}

