/*
 * 
 * Copyright (C) 2016 Alfabook srl
 * License: http://www.gnu.org/licenses/gpl-2.0.txt GNU General Public License v2
 *
*/

#define UPDATE_STATUS_FILE "/var/cache/microninja-updater/status.dat"

typedef enum
{
	CHECKING_FOR_URGENT_UPDATES,	
    CHECKING_FOR_NORMAL_UPDATES,
    NO_UPDATES_AVAILABLE,
    UPDATES_AVAILABLE,
    DOWNLOADING_UPDATES,
    SYSTEM_UPDATED
} UpdatingStatus;
