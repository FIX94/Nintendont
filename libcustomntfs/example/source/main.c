/**
 * main.c - Directory listing example for NTFS-based devices.
 *
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <gctypes.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <fat.h>
#include <ntfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void list(const char *path, int depth)
{
    DIR *pdir;
    struct dirent *pent;
    struct stat st;
    char indent[PATH_MAX] = {0};
    char new_path[PATH_MAX] = {0};

    // Open the directory
    pdir = opendir(path);
    if (pdir) {

        // Make this our current directory
        chdir(path);

        // Build a directory indent (for better readability)
        memset(indent, ' ', depth * 2);

        // List the contents of the directory
        while ((pent = readdir(pdir)) != NULL) {
            if ((strcmp(pent->d_name, ".") == 0) || (strcmp(pent->d_name, "..") == 0))
                continue;

            // Get the entries stats
            if (stat(pent->d_name, &st) == -1)
                continue;

            // List the entry
            if (S_ISDIR(st.st_mode)) {
                printf(" D %s%s/\n", indent, pent->d_name);

                // List the directories contents
                sprintf(new_path, "%s/%s", path, pent->d_name);
                list(new_path, depth + 1);
                chdir(path);

            } else if (S_ISREG(st.st_mode)) {
                printf(" F %s%s (%lu)\n", indent, pent->d_name, (unsigned long int)st.st_size);
            } else {
                printf(" ? %s%s\n", indent, pent->d_name);
            }

        }

        // Close the directory
        closedir(pdir);

    } else {
        printf("opendir(%s) failure.\n", path);
    }

    return;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

    // Initialise the video system
    VIDEO_Init();

    // This function initialises the attached controllers
    WPAD_Init();

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Initialise the console, required for printf
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    // Set up the video registers with the chosen mode
    VIDEO_Configure(rmode);

    // Tell the video hardware where our display memory is
    VIDEO_SetNextFramebuffer(xfb);

    // Make the display visible
    VIDEO_SetBlack(FALSE);

    // Flush the video register changes to the hardware
    VIDEO_Flush();

    // Wait for Video setup to complete
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();


    // The console understands VT terminal escape codes
    // This positions the cursor on row 2, column 0
    // we can use variables for this with format codes too
    // e.g. printf ("\x1b[%d;%dH", row, column );
    printf("\x1b[2;0H");

    printf("\n");
    printf(" NTFS Directory Listing Example\n");
    printf("\n");
    printf(" - 'LEFT' and 'RIGHT' to select a volume\n");
    printf(" - 'A' to enumerate the selected volume\n");
    printf(" - 'HOME' to quit\n");
    printf("\n");

    bool listed = false;
    ntfs_md *mounts = NULL;
    int mountCount = 0;
    int mountIndex = 0;
    int i;

    // Mount FAT devices
    fatInitDefault();

    // Mount all NTFS volumes on all inserted block devices
    mountCount = ntfsMountAll(&mounts, NTFS_DEFAULT | NTFS_RECOVER);
    if (mountCount == -1)
        printf("Error whilst mounting devices (%i).\n", errno);
    else if (mountCount == 0)
        printf("No NTFS volumes were found and/or mounted.\n");
    else
        printf("%i NTFS volumes(s) mounted!\n\n", mountCount);

    // List all mounted NTFS volumes
    for (i = 0; i < mountCount; i++)
        printf("%i - %s:/ (%s)\n", i + 1, mounts[i].name, ntfsGetVolumeName(mounts[i].name));

    printf("\n");
    while (1) {

        // Call WPAD_ScanPads each loop, this reads the latest controller states
        WPAD_ScanPads();

        // WPAD_ButtonsDown tells us which buttons were pressed in this loop
        // this is a "one shot" state which will not fire again until the button has been released
        u32 pressed = WPAD_ButtonsDown(0);

        // Break from main loop
        if (pressed & WPAD_BUTTON_HOME) break;

        // If there is at least one volume mounted and we have not yet listed one...
        if (mountCount > 0 && !listed) {

            // Deincrement the selected volumes index
            if (pressed & WPAD_BUTTON_LEFT)
                mountIndex = MIN(MAX(mountIndex - 1, 0), mountCount - 1);

            // Increment the selected volumes index
            if (pressed & WPAD_BUTTON_RIGHT)
                mountIndex = MIN(MAX(mountIndex + 1, 0), mountCount - 1);

            // Enumerate the selected volumes contents
            if (pressed & WPAD_BUTTON_A) {
                printf("\n\n");

                // List the volumes root directory
                char path[PATH_MAX] = {0};
                strcpy(path, mounts[mountIndex].name);
                strcat(path, ":/");
                list(path, 0);
                listed = true;

                printf("\n");
                printf("Press 'HOME' to quit.\n\n");

            }

            // If we have not listed a volume yet then prompt to select one
            if(!listed) {
                printf("\rSelect a NTFS volume: < %i >", mountIndex + 1);
            }

        }

        // Wait for the next frame
        VIDEO_WaitVSync();

    }

    // Unmount all NTFS volumes and clean up
    if (mounts) {
        for (i = 0; i < mountCount; i++)
            ntfsUnmount(mounts[i].name, true);
        free(mounts);
    }

    // We return to the launcher application via exit
    exit(0);

    return 0;
}

