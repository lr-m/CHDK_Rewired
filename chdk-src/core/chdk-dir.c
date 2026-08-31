/*
 * FAT12/16/32 LFN-aware opendir, readdir, closedir implementation
 *
 * Copyright (C) 2014 srsa @ CHDK Forum
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * Only a restricted set of ascii characters is supported in long filenames, falls back to short names otherwise
 * Uses less memory than the cameras' ReadFDir functions
 * v 1.1: removed CHDK wrappers
 * v 1.2: - add basic filename and total path length limit check (return short names when they are exceeded)
 *        - bugfix: ignore terminating zero char of LFN
 */

#include "stdlib.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define FNMAX 107
#define DCSIZE 16384 // read cache size, 128 for testing, something like 16384 later

typedef struct
{
    int     fd;                 // directory file descriptor
    char*   dc;                 // allocated directory cache
    int     cp;                 // directory cache pointer used by the cache routines
    int     rc;                 // read count

    union
    {
        char          *fe;      // currently evaluated FAT entry
        unsigned char *feu;
    };
    int     islfn;              // long file name is being read (state)
    int     lfnpos;             // position in the long output string (starts from 1 due to the terminating zero char)

    int     mnl;                // maximum name length

    char    fn[FNMAX+1];        // current file name stored here by CHDKReadDir
} myDIR_s;

static int read_next_entry(myDIR_s* dir)
{
    int rc = 0;
    if ( (dir->cp==-1) || (dir->cp>=MIN(DCSIZE, dir->rc)) ) // cache empty or fully read out
    {
        rc = read(dir->fd, dir->dc, DCSIZE);
        if ( rc < 32 ) // short read or error
        {
            return rc;
        }
        dir->rc = rc;
        dir->fe = dir->dc;
        dir->cp = 32;
        return 32;
    }
    else    // read from cache
    {
        int canread = MIN(dir->rc - dir->cp, 32);
        dir->fe = dir->dc + dir->cp;
        dir->cp += canread;
        return canread;
    }
}

static void rewind_entry(myDIR_s* dir)
{
    dir->cp -= 32;
}

void *CHDKOpenDir(const char* name)
{
    myDIR_s *dir = malloc( sizeof(myDIR_s) );
    if ( dir )
    {
        char *dirc = umalloc(DCSIZE);
        if ( dirc )
        {
            dir->fd = open(name, 0, 0x124);
            if ( dir->fd != -1 )
            {
                dir->fn[0] = 0;
                dir->dc = dirc;
                dir->cp = -1;
                // determine name length limit ('-1' is due to the extra '/' in a full filename)
                dir->mnl = MIN(CAM_MAX_FNAME_LENGTH,CAM_MAX_PATH_LENGTH-strlen(name)-1);
                return dir;
            }
            else
            {
                free((void*)dir);
                ufree((void*)dirc);
            }
        }
        else
        {
            free((void*)dir);
        }
    }
    return NULL; // failure
}

int CHDKCloseDir(void *d)
{
    myDIR_s *dir = d;
    // if ( d ) // commented out, the wrapper already checks this
    {
        int ret = close(dir->fd);
        ufree(dir->dc);
        free(d);
        return ret;
    }
    return -1;
}

const unsigned char lfnchpos[]={30,28,24,22,20,18,16,14,9,7,5,3,1};

int check_fn_char(int i)
{
    if ( i & 0xffffff80 ) return -1;
    if (
        ((i >= '0') && (i <= '9')) ||
        ((i >= '@'/*'A'*/) && (i <= 'Z')) ||
        ((i >= 'a') && (i <= 'z')) ||
        ((i == '.') || (i == '-') || (i == '_') || (i == '(') || (i == ')') || (i == '$') || (i == '&'))
       )
    {
        return i;
    }
    if ( i != 0 ) return -1;
    return 0;
}

void read_lfn_entry(myDIR_s* dir)
{
    int n;
    for (n=0; n<13; n++)
    {
        // read unicode char, reading as halfword is not working on armv5 due to alignment
        int uch = *(unsigned char*)(dir->fe+lfnchpos[n])+((*(unsigned char*)(dir->fe+lfnchpos[n]+1))<<8);
        if ((uch != 0xffff) && (uch != 0))  // unused space is filled with '0xffff' chars and zero or one '0x0' char
        {
            if ( (check_fn_char(uch) < 0) ) // disable lfn if any chars are outside 7bit ascii
            {
                dir->islfn = 0;
                break;
            }
            dir->fn[FNMAX-dir->lfnpos] = (char)uch;
            dir->lfnpos++;
        }
    }
}

int CHDKReadDir(void *d, void* dd)
{
    myDIR_s *dir = d;
    int rd;
    int lfnchsum = 0;    // lfn checksum (only zeroed here to calm down the compiler)
    dir->islfn = 0;      // long file name is being read (state)
    dir->lfnpos = 0;     // position in the long output string (only zeroed here to calm down the compiler)

    //if ( (int)d && (int)dd ) // commented out, the wrapper already checks this
    {
        while (1)
        {
            rd=read_next_entry(dir);
            if ( (rd < 32) ) // either error or short read, return with null string
            {
                break;
            }
            if ( dir->fe[0] == 0 ) // last entry, return with null string
            {
                break;
            }
            if ( (dir->feu[0] == 0xe5) || ((dir->fe[11]&0xf) == 8) ) // erased entry or label, makes LFN invalid -> skip entry
            {
                dir->islfn = 0;
                continue;
            }
            if ( !dir->islfn && (dir->fe[11] != 0xf) ) // no LFN in progress, not LFN entry -> read short name, return
            {
                int n, m;
                m = 0; // position in output
                for (n=0; n<8; n++) // name
                {
                    if ( (n>0) && (dir->fe[n]==0x20) ) break;
                    dir->fn[m] = dir->fe[n];
                    m++;
                }
                if ( (dir->fe[8]!=0x20) ) // add dot only when there's extension
                {
                    dir->fn[m] = '.';
                    m++;
                }
                for (n=8; n<11; n++) // extension
                {
                    if ( (dir->fe[n]==0x20) ) break;
                    dir->fn[m] = dir->fe[n];
                    m++;
                }
                dir->fn[m] = 0;
                strcpy(dd, dir->fn);
                return (int)(dir->fn);
            }
            if ( (dir->islfn == 1) && (dir->fe[11] != 0xf) ) // lfn entries over, this must be the short filename entry
            {
                // compute checksum
                unsigned char cs = 0;
                int n;
                for (n = 0; n < 11; n++)
                {
                    cs = (((cs & 1) << 7) | ((cs & 0xfe) >> 1)) + dir->feu[n];
                }
                // checksum computed
                if ( (cs == lfnchsum) && (dir->lfnpos-1 <= dir->mnl) ) 
                {
                    // lfn is valid, not too long, and belongs to this short name -> return
                    strcpy(dd, (dir->fn)+FNMAX-dir->lfnpos+1);
                    return (int)((dir->fn)+FNMAX-dir->lfnpos+1);
                }
                else // invalid checksum or name too long, try re-interpreting entry
                {
                    rewind_entry(dir);
                    dir->islfn = 0;
                    continue;
                }
            }
            else if ( dir->fe[11] == 0xf ) // lfn entry, process or skip
            {
                if (dir->islfn) // already in an lfn block
                {
                    if (                         // check for anomalies:
                         (dir->feu[13] != lfnchsum) || // checksum doesn't match
                         (dir->fe[0] & 0x40) ||       // first entry of an lfn block
                         (dir->fe[0] != dir->islfn-1)      // out of order entry
                       )
                    {
                        dir->islfn = 0;
                        continue;
                    }
                    dir->islfn = dir->fe[0]; // number of lfn entries left + 1
                    read_lfn_entry(dir);
                    if ( dir->lfnpos > 99+1 ) // CHDK limit (100 chars) hit, skip lfn
                    {
                        dir->islfn = 0;
                        continue;
                    }
                }
                else // lfn block start
                {
                    if ( (dir->fe[0] & 0x40) && (dir->fe[0]-0x40 <= 8) ) // start must be valid and name not too long
                    {
                        dir->islfn = dir->fe[0] - 0x40; // number of lfn entries left + 1
                        memset(dir->fn, 0, FNMAX+1);
                        dir->lfnpos = 1; // name will be filled backwards, the last char of the buffer will remain 0 for safety
                        read_lfn_entry(dir);
                        lfnchsum = dir->feu[13];
                    }
                    else // invalid or too long, skip
                    {
                        continue;
                    }
                }
            }
            else // lfn invalid, skip
            {
                dir->islfn = 0;
            }
        }
// problem or end-of-directory
        dir->fn[0] = 0;
        strcpy(dd, dir->fn);
    }
    return 0;
}
