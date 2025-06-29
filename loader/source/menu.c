/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include <gccore.h>
#include <sys/param.h>
#include "font.h"
#include "exi.h"
#include "global.h"
#include "FPad.h"
#include "Config.h" // Should include CommonConfig.h
#include "update.h"
#include "titles.h"
#include "dip.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogc/stm.h>
#include <ogc/consol.h>
#include <ogc/system.h>
//#include <fat.h>
#include <di/di.h>

#include "menu.h"
#include "../../common/include/CommonConfigStrings.h" // Contains VideoModeStrings and NUM_VIDEOMODE_STRINGS
#include "ff_utf8.h"
#include "ShowGameInfo.h"

// Dark gray for grayed-out menu items.
#define DARK_GRAY 0x666666FF

// Device state.
typedef enum {
        // Device is open and has GC titles in "games" directory.
        DEV_OK = 0,
        // Device could not be opened.
        DEV_NO_OPEN = 1,
        // Device was opened but has no "games" directory.
        DEV_NO_GAMES = 2,
        // Device was opened but "games" directory is empty.
        DEV_NO_TITLES = 3,
} DevState;
static u8 devState = DEV_OK;

// Disc format colors.
const u32 DiscFormatColors[8] =
{
        BLACK,          // Full
        0x551A00FF,     // Shrunken (dark brown)
        0x00551AFF,     // Extracted FST
        0x001A55FF,     // CISO
        0x551A55FF,     // Multi-Game
        0x55551AFF,     // Oversized (dark yellow)
        GRAY,           // undefined
        GRAY,           // undefined
};

/**
 * Print information about the selected device.
 */
static void PrintDevInfo(void);

extern NIN_CFG* ncfg;

u32 Shutdown = 0;
extern char *launch_dir;

// Map settings menu index for "Force Videomode" to actual video mode flags
// This order must match VideoModeStrings[] in CommonConfigStrings.h
static const u32 videoModeFlagMap[] = {
    NIN_VID_FORCE_PAL50,        // Index 0
    NIN_VID_FORCE_PAL60,        // Index 1
    NIN_VID_FORCE_NTSC,         // Index 2
    NIN_VID_FORCE_MPAL,         // Index 3
    NIN_VID_FORCE_NTSC_240P,    // Index 4
    NIN_VID_FORCE_PAL_288P,     // Index 5
    NIN_VID_FORCE_MPAL_240P,    // Index 6
    NIN_VID_FORCE_EURGB60_240P, // Index 7
    NIN_VID_FORCE_PAL_576P      // Index 8
};


inline u32 SettingY(u32 row)
{
        return 127 + 16 * row;
}
void SetShutdown(void)
{
        Shutdown = 1;
}
void HandleWiiMoteEvent(s32 chan)
{
        SetShutdown();
}
void HandleSTMEvent(u32 event)
{
        *(vu32*)(0xCC003024) = 1;

        switch(event)
        {
                default:
                case STM_EVENT_RESET:
                        break;
                case STM_EVENT_POWER:
                        SetShutdown();
                        break;
        }
}
int compare_names(const void *a, const void *b)
{
        const gameinfo *da = (const gameinfo *) a;
        const gameinfo *db = (const gameinfo *) b;

        int ret = strcasecmp(da->Name, db->Name);
        if (ret == 0)
        {
                // Names are equal. Check disc number.
                const uint8_t dnuma = (da->Flags & GIFLAG_DISCNUMBER_MASK);
                const uint8_t dnumb = (db->Flags & GIFLAG_DISCNUMBER_MASK);
                if (dnuma < dnumb)
                        ret = -1;
                else if (dnuma > dnumb)
                        ret = 1;
                else
                        ret = 0;
        }
        return ret;
}

/**
 * Check if a disc image is valid.
 * @param filename      [in]  Disc image filename. (ISO/GCM)
 * @param discNumber    [in]  Disc number.
 * @param gi            [out] gameinfo struct to store game information if the disc is valid.
 * @return True if the image is valid; false if not.
 */
static bool IsDiscImageValid(const char *filename, int discNumber, gameinfo *gi)
{
        // TODO: Handle FST format (sys/boot.bin).
        u8 buf[0x100];                  // Disc header.
        u32 BI2region_addr = 0x458;     // BI2 region code address.

        FIL in;
        if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK)
        {
                // Could not open the disc image.
                return false;
        }

        // Read the disc header
        //gprintf("(%s) ok\n", filename );
        bool ret = false;
        UINT read;
        f_read(&in, buf, sizeof(buf), &read);
        if (read != sizeof(buf))
        {
                // Error reading from the file.
                f_close(&in);
                return false;
        }

        // Check for CISO magic with 2 MB block size.
        // NOTE: CISO block size is little-endian.
        static const uint8_t CISO_MAGIC[8] = {'C','I','S','O',0x00,0x00,0x20,0x00};
        if (!memcmp(buf, CISO_MAGIC, sizeof(CISO_MAGIC)) &&
            !IsGCGame(buf))
        {
                // CISO magic is present, and GCN magic isn't.
                // This is most likely a CISO image.
                // Read the actual GCN header.
                f_lseek(&in, 0x8000);
                f_read(&in, buf, 0x100, &read);
                if (read != 0x100)
                {
                        // Error reading from the file.
                        f_close(&in);
                        return false;
                }

                // Adjust the BI2 region code address for CISO.
                BI2region_addr = 0x8458;

                gi->Flags = GIFLAG_FORMAT_CISO;
        }
        else
        {
                // Standard GameCube disc image.
                // TODO: Detect Triforce images and exclude them
                // from size checking?
                if (in.obj.objsize == 1459978240)
                {
                        // Full 1:1 GameCube image.
                        gi->Flags = GIFLAG_FORMAT_FULL;
                }
                else if (in.obj.objsize > 1459978240)
                {
                        // Oversized GameCube image.
                        gi->Flags = GIFLAG_FORMAT_OVER;
                }
                else
                {
                        // Shrunken GameCube image.
                        gi->Flags = GIFLAG_FORMAT_SHRUNKEN;
                }
        }

        if (IsGCGame(buf))      // Must be GC game
        {
                // Read the BI2 region code.
                u32 BI2region;
                f_lseek(&in, BI2region_addr);
                f_read(&in, &BI2region, sizeof(BI2region), &read);
                if (read != sizeof(BI2region)) {
                        // Error reading from the file.
                        f_close(&in);
                        return false;
                }

                // Save the region code for later.
                gi->Flags |= ((BI2region & 3) << 3);

                // Save the game ID.
                memcpy(gi->ID, buf, 6); //ID for EXI
                gi->Flags |= (discNumber & 3) << 5;

                // Save the revision number.
                gi->Revision = buf[0x07];

                // Check if this is a multi-game image.
                // Reference: https://gbatemp.net/threads/wit-wiimms-iso-tools-gamecube-disc-support.251630/#post-3088119
                const bool is_multigame = IsMultiGameDisc((const char*)buf);
                if (is_multigame)
                {
                        if (gi->Flags == GIFLAG_FORMAT_CISO)
                        {
                                // Multi-game + CISO is NOT supported.
                                ret = false;
                        }
                        else
                        {
                                // Multi-game disc.
                                char *name = (char*)malloc(65);
                                const char *slash_pos = strrchr(filename, '/');
                                snprintf(name, 65, "Multi-Game Disc (%s)", (slash_pos ? slash_pos+1 : filename));
                                gi->Name = name;
                                gi->Flags = GIFLAG_FORMAT_MULTI | GIFLAG_NAME_ALLOC;
                        }
                }
                else
                {
                        // Check if this title is in titles.txt.
                        bool isTriforce;
                        const char *dbTitle = SearchTitles(gi->ID, &isTriforce);
                        if (dbTitle)
                        {
                                // Title found.
                                gi->Name = (char*)dbTitle;
                                gi->Flags &= ~GIFLAG_NAME_ALLOC;

                                if (isTriforce)
                                {
                                        // Clear the format value if it's "shrunken",
                                        // since Triforce titles are never the size
                                        // of a full 1:1 GameCube disc image.
                                        if ((gi->Flags & GIFLAG_FORMAT_MASK) == GIFLAG_FORMAT_SHRUNKEN)
                                        {
                                                gi->Flags &= ~GIFLAG_FORMAT_MASK;
                                        }
                                }
                        }

                        if (!dbTitle)
                        {
                                // Title not found.
                                // Use the title from the disc header.

                                // Make sure the title in the header is NULL terminated.
                                buf[0x20+65] = 0;
                                gi->Name = strdup((const char*)&buf[0x20]);
                                gi->Flags |= GIFLAG_NAME_ALLOC;
                        }
                }

                gi->Path = strdup(filename);
                ret = true;
        }

        f_close(&in);
        return ret;
}

/**
 * Get all games from the games/ directory on the selected storage device.
 * On Wii, this also adds a pseudo-game for loading GameCube games from disc.
 *
 * @param gi           [out] Array of gameinfo structs.
 * @param sz           [in]  Maximum number of elements in gi.
 * @param pGameCount   [out] Number of games loaded. (Includes GCN pseudo-game for Wii.)
 *
 * @return DevState value:
 * - DEV_OK: Device opened and has GC titles in "games/" directory.
 * - DEV_NO_OPEN: Could not open the storage device.
 * - DEV_NO_GAMES: No "games/" directory was found.
 * - DEV_NO_TITLES: "games/" directory contains no GC titles.
 */
static DevState LoadGameList(gameinfo *gi, u32 sz, u32 *pGameCount)
{
        // Create a list of games
        char filename[MAXPATHLEN];      // Current filename.
        u8 buf[0x100];                  // Disc header.
        int gamecount = 0;              // Current game count.
        u32 i;
        char dir_path[256];
        struct stat st;


        if( isWiiVC )
        {
                // Pseudo game for booting a GameCube disc on Wii VC.
                gi[0].ID[0] = 'D',gi[0].ID[1] = 'I',gi[0].ID[2] = 'S';
                gi[0].ID[3] = 'C',gi[0].ID[4] = '0',gi[0].ID[5] = '1';
                gi[0].Name = "Boot included GC Disc";
                gi[0].Revision = 0;
                gi[0].Flags = 0;
                gi[0].Path = strdup("di:di");
                gamecount++;
        }
        else if( !IsWiiU() )
        {
                // Pseudo game for booting a GameCube disc on Wii.
                gi[0].ID[0] = 'D',gi[0].ID[1] = 'I',gi[0].ID[2] = 'S';
                gi[0].ID[3] = 'C',gi[0].ID[4] = '0',gi[0].ID[5] = '1';
                gi[0].Name = "Boot GC Disc in Drive";
                gi[0].Revision = 0;
                gi[0].Flags = 0;
                gi[0].Path = strdup("di:di");
                gamecount++;
        }

        DIR pdir;
        snprintf(filename, sizeof(filename), "%s:/games", GetRootDevice());
        if (f_opendir_char(&pdir, filename) != FR_OK)
        {
                // Could not open the "games" directory.

                // Attempt to open the device root.
                snprintf(filename, sizeof(filename), "%s:/", GetRootDevice());
                if (f_opendir_char(&pdir, filename) != FR_OK)
                {
                        // Could not open the device root.
                        if (pGameCount)
                                *pGameCount = 0;
                        return DEV_NO_OPEN;
                }

                // Device root opened.
                // This means the device is usable, but it
                // doesn't have a "games" directory.
                f_closedir(&pdir);
                if (pGameCount)
                        *pGameCount = gamecount;
                return DEV_NO_GAMES;
        }

        // Process the directory.
        FILINFO fInfo;
        FIL in;
        while (f_readdir(&pdir, &fInfo) == FR_OK && fInfo.fname[0] != '\0')
        {
                if (fInfo.fname[0] == '.')
                        continue;

                if (fInfo.fattrib & AM_DIR)
                {
                        const char *filename_utf8 = wchar_to_char(fInfo.fname);
                        int fnlen = snprintf(filename, sizeof(filename), "%s:/games/%s/",
                                             GetRootDevice(), filename_utf8);
                        bool found = false;
                        static const char disc_filenames[8][16] = {
                                "game.ciso", "game.cso", "game.gcm", "game.iso",
                                "disc2.ciso", "disc2.cso", "disc2.gcm", "disc2.iso"
                        };
                        for (i = 0; i < 8; i++)
                        {
                                const u32 discNumber = i / 4;
                                strcpy(&filename[fnlen], disc_filenames[i]);
                                if (IsDiscImageValid(filename, discNumber, &gi[gamecount]))
                                {
                                        gamecount++;
                                        found = true;
                                        i = (discNumber * 4) + 3;
                                }
                        }
                        if (!found)
                        {
                                memcpy(&filename[fnlen], "sys/boot.bin", 13);
                                if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK)
                                        continue;
                                UINT read_len;
                                f_read(&in, buf, 0x100, &read_len);
                                f_close(&in);
                                if (read_len != 0x100 || !IsGCGame(buf))
                                        continue;
                                memcpy(&filename[fnlen], "sys/bi2.bin", 12);
                                if (f_open_char(&in, filename, FA_READ|FA_OPEN_EXISTING) != FR_OK)
                                        continue;
                                u32 BI2region_val;
                                f_lseek(&in, 0x18);
                                f_read(&in, &BI2region_val, sizeof(BI2region_val), &read_len);
                                f_close(&in);
                                if (read_len != sizeof(BI2region_val))
                                        continue;
                                filename[fnlen] = 0;
                                buf[0x20+65] = 0;
                                memcpy(gi[gamecount].ID, buf, 6);
                                gi[gamecount].Revision = 0;
                                gi[gamecount].Name = strdup((const char*)&buf[0x20]);
                                gi[gamecount].Flags = GIFLAG_NAME_ALLOC | GIFLAG_FORMAT_FST | ((BI2region_val & 3) << 3);
                                gi[gamecount].Path = strdup(filename);
                                gamecount++;
                        }
                }
                else
                {
                        const char *filename_utf8 = wchar_to_char(fInfo.fname);
                        if (IsSupportedFileExt(filename_utf8))
                        {
                                snprintf(filename, sizeof(filename), "%s:/games/%s",
                                         GetRootDevice(), filename_utf8);
                                if (IsDiscImageValid(filename, 0, &gi[gamecount]))
                                {
                                        gamecount++;
                                }
                        }
                }
                if (gamecount >= sz)
                        break;
        }
        f_closedir(&pdir);

        if( gamecount && IsWiiU() && !isWiiVC )
                qsort(gi, gamecount, sizeof(gameinfo), compare_names);
        else if( gamecount > 1 )
                qsort(&gi[1], gamecount-1, sizeof(gameinfo), compare_names);

        if (pGameCount)
                *pGameCount = gamecount;

        if(gamecount == 0)
                return DEV_NO_TITLES;
        return DEV_OK;
}

// Menu selection context.
typedef struct _MenuCtx
{
        u32 menuMode;
        bool redraw;
        bool selected;
        bool saveSettings;
        struct {
                u32 Up;
                u32 Down;
                u32 Left;
                u32 Right;
        } held;
        struct {
                s32 posX;
                s32 scrollX;
                u32 listMax;
                const gameinfo *gi;
                int gamecount;
                bool canBeBooted;
                bool canShowInfo;
        } games;
        struct {
                u32 settingPart;
                s32 posX;
        } settings;
} MenuCtx;

#define FPAD_WRAPPER_REPEAT(Key) \
static inline int FPAD_##Key##_Repeat(MenuCtx *ctx) \
{ \
        int ret = 0; \
        if (FPAD_##Key(1)) { \
                ret = (ctx->held.Key == 0 || ctx->held.Key > 10); \
                ctx->held.Key++; \
        } else { \
                ctx->held.Key = 0; \
        } \
        return ret; \
}
FPAD_WRAPPER_REPEAT(Up)
FPAD_WRAPPER_REPEAT(Down)
FPAD_WRAPPER_REPEAT(Left)
FPAD_WRAPPER_REPEAT(Right)

static bool UpdateGameSelectMenu(MenuCtx *ctx)
{
        u32 i;
        bool clearCheats = false;

        if(FPAD_X(0))
        {
                if (ctx->games.canShowInfo)
                {
                        ShowGameInfo(&ctx->games.gi[ctx->games.posX + ctx->games.scrollX]);
                        ctx->redraw = true;
                }
        }
        if (FPAD_Down_Repeat(ctx))
        {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );
                if (ctx->games.posX + 1 >= ctx->games.listMax)
                {
                        if (ctx->games.posX + 1 + ctx->games.scrollX < ctx->games.gamecount) {
                                ctx->games.scrollX++;
                        } else {
                                ctx->games.posX = 0;
                                ctx->games.scrollX = 0;
                        }
                } else {
                        ctx->games.posX++;
                }
                clearCheats = true;
                ctx->redraw = true;
                ctx->saveSettings = true;
        }
        if (FPAD_Right_Repeat(ctx))
        {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );
                if (ctx->games.posX == ctx->games.listMax - 1)
                {
                        if (ctx->games.posX + ctx->games.listMax + ctx->games.scrollX < ctx->games.gamecount) {
                                ctx->games.scrollX += ctx->games.listMax;
                        } else if (ctx->games.posX + ctx->games.scrollX != ctx->games.gamecount - 1) {
                                ctx->games.scrollX = ctx->games.gamecount - ctx->games.listMax;
                        } else {
                                ctx->games.posX = 0;
                                ctx->games.scrollX = 0;
                        }
                } else if(ctx->games.listMax) {
                        ctx->games.posX = ctx->games.listMax - 1;
                }
                else {
                        ctx->games.posX = 0;
                }
                clearCheats = true;
                ctx->redraw = true;
                ctx->saveSettings = true;
        }
        if (FPAD_Up_Repeat(ctx))
        {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );
                if (ctx->games.posX <= 0)
                {
                        if (ctx->games.scrollX > 0) {
                                ctx->games.scrollX--;
                        } else {
                                if(ctx->games.listMax) {
                                        ctx->games.posX = ctx->games.listMax - 1;
                                }
                                else {
                                        ctx->games.posX = 0;
                                }
                                ctx->games.scrollX = ctx->games.gamecount - ctx->games.listMax;
                        }
                } else {
                        ctx->games.posX--;
                }
                clearCheats = true;
                ctx->redraw = true;
                ctx->saveSettings = true;
        }
        if (FPAD_Left_Repeat(ctx))
        {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X+51*6-8, MENU_POS_Y + 20*6 + ctx->games.posX * 20, " " );
                if (ctx->games.posX == 0)
                {
                        if (ctx->games.scrollX - (s32)ctx->games.listMax >= 0) {
                                ctx->games.scrollX -= ctx->games.listMax;
                        } else if (ctx->games.scrollX != 0) {
                                ctx->games.scrollX = 0;
                        } else {
                                ctx->games.scrollX = ctx->games.gamecount - ctx->games.listMax;
                        }
                } else {
                        ctx->games.posX = 0;
                }
                clearCheats = true;
                ctx->redraw = true;
                ctx->saveSettings = true;
        }
        if (FPAD_OK(0) && ctx->games.canBeBooted)
        {
                ctx->selected = true;
                return true;
        }
        if (clearCheats)
        {
                if ((ncfg->Config & NIN_CFG_CHEATS) && (ncfg->Config & NIN_CFG_CHEAT_PATH))
                {
                        ncfg->Config = ncfg->Config & ~(NIN_CFG_CHEATS | NIN_CFG_CHEAT_PATH);
                }
        }
        if (ctx->redraw)
        {
                PrintFormat(DEFAULT_SIZE, DiscFormatColors[0], MENU_POS_X, MENU_POS_Y + 20*3, "Colors  : 1:1");
                PrintFormat(DEFAULT_SIZE, DiscFormatColors[1], MENU_POS_X+(14*10), MENU_POS_Y + 20*3, "Shrunk");
                PrintFormat(DEFAULT_SIZE, DiscFormatColors[2], MENU_POS_X+(21*10), MENU_POS_Y + 20*3, "FST");
                PrintFormat(DEFAULT_SIZE, DiscFormatColors[3], MENU_POS_X+(25*10), MENU_POS_Y + 20*3, "CISO");
                PrintFormat(DEFAULT_SIZE, DiscFormatColors[4], MENU_POS_X+(30*10), MENU_POS_Y + 20*3, "Multi");
                PrintFormat(DEFAULT_SIZE, DiscFormatColors[5], MENU_POS_X+(36*10), MENU_POS_Y + 20*3, "Over");
                int gamelist_y = MENU_POS_Y + 20*5 + 10;
                const gameinfo *cur_gi = &ctx->games.gi[ctx->games.scrollX];
                int gamesToPrint = ctx->games.gamecount - ctx->games.scrollX;
                if (gamesToPrint > ctx->games.listMax) {
                        gamesToPrint = ctx->games.listMax;
                }
                for (i = 0; i < gamesToPrint; ++i, gamelist_y += 20, cur_gi++)
                {
                        const u32 color = DiscFormatColors[cur_gi->Flags & GIFLAG_FORMAT_MASK];
                        const u8 discNumber = ((cur_gi->Flags & GIFLAG_DISCNUMBER_MASK) >> 5);
                        if (discNumber == 0)
                        {
                                PrintFormat(DEFAULT_SIZE, color, MENU_POS_X, gamelist_y,
                                            "%50.50s [%.6s]%s",
                                            cur_gi->Name, cur_gi->ID,
                                            i == ctx->games.posX ? ARROW_LEFT : " ");
                        }
                        else
                        {
                                PrintFormat(DEFAULT_SIZE, color, MENU_POS_X, gamelist_y,
                                            "%46.46s (%d) [%.6s]%s",
                                            cur_gi->Name, discNumber+1, cur_gi->ID,
                                            i == ctx->games.posX ? ARROW_LEFT : " ");
                        }
                }

                if(ctx->games.gamecount && (ctx->games.scrollX + ctx->games.posX) >= 0
                        && (ctx->games.scrollX + ctx->games.posX) < ctx->games.gamecount)
                {
                        ctx->games.canBeBooted = true;
                        if (IsWiiU() && !isWiiVC) {
                                ctx->games.canShowInfo = true;
                        } else {
                                if ((ctx->games.scrollX + ctx->games.posX) == 0) {
                                        ctx->games.canShowInfo = false;
                                } else {
                                        ctx->games.canShowInfo = true;
                                }
                        }

                        if (ctx->games.canShowInfo) {
                                const gameinfo *const gi_sel = &ctx->games.gi[ctx->games.scrollX + ctx->games.posX];
                                const int len = strlen(gi_sel->Path);
                                const int x = (640 - (len*10)) / 2;
                                const u32 color = DiscFormatColors[gi_sel->Flags & GIFLAG_FORMAT_MASK];
                                PrintFormat(DEFAULT_SIZE, color, x, MENU_POS_Y + 20*4+5, "%s", gi_sel->Path);
                        }
                }
                else
                {
                        ctx->games.canBeBooted = false;
                        ctx->games.canShowInfo = false;
                }
        }
        return false;
}

static const char *const *GetSettingsDescription(const MenuCtx *ctx)
{
        if (ctx->settings.settingPart == 0)
        {
                switch (ctx->settings.posX)
                {
                        case NIN_CFG_BIT_CHEATS:
                        case NIN_CFG_BIT_DEBUGGER:
                        case NIN_CFG_BIT_DEBUGWAIT:
                                break;
                        case NIN_CFG_BIT_MEMCARDEMU: {
                                static const char *desc_mcemu[] = {
                                        "Emulates a memory card in",
                                        "Slot A using a .raw file.",
                                        "",
                                        "Disable this option if you",
                                        "want to use a real memory",
                                        "card on an original Wii.",
                                        NULL
                                };
                                return desc_mcemu;
                        }
                        case NIN_CFG_BIT_CHEAT_PATH:
                                break;
                        case NIN_CFG_BIT_FORCE_WIDE: {
                                static const char *const desc_force_wide[] = {
                                        "Patch games to use a 16:9",
                                        "aspect ratio. (widescreen)",
                                        "",
                                        "Not all games support this",
                                        "option. The patches will not",
                                        "be applied to games that have",
                                        "built-in support for 16:9;",
                                        "use the game's options screen",
                                        "to configure the display mode.",
                                        NULL
                                };
                                return desc_force_wide;
                        }
                        case NIN_CFG_BIT_FORCE_PROG: {
                                static const char *const desc_force_prog[] = {
                                        "Patch games to always use 480p",
                                        "(progressive scan) output.",
                                        "",
                                        "Requires component cables, or",
                                        "an HDMI cable on Wii U.",
                                        NULL
                                };
                                return desc_force_prog;
                        }
                        case NIN_CFG_BIT_AUTO_BOOT:
                                break;
                        case NIN_CFG_BIT_REMLIMIT: {
                                static const char *desc_remlimit[] = {
                                        "Disc read speed is normally",
                                        "limited to the performance of",
                                        "the original GameCube disc",
                                        "drive.",
                                        "",
                                        "Unlocking read speed can allow",
                                        "for faster load times, but it",
                                        "can cause problems with games",
                                        "that are extremely sensitive",
                                        "to disc read timing.",
                                        NULL
                                };
                                return desc_remlimit;
                        }
                        case NIN_CFG_BIT_OSREPORT:
                                break;
                        case NIN_CFG_BIT_USB: {
                                static const char *desc_wiiu_widescreen[] = {
                                        "On Wii U, Nintendont sets the",
                                        "display to 4:3, which results",
                                        "in bars on the sides of the",
                                        "screen. If playing a game that",
                                        "supports widescreen, enable",
                                        "this option to set the display",
                                        "back to 16:9.",
                                        "",
                                        "This option has no effect on",
                                        "original Wii systems.",
                                        NULL
                                };
                                return desc_wiiu_widescreen;
                        }
                        case NIN_CFG_BIT_LED: {
                                static const char *desc_led[] = {
                                        "Use the drive slot LED as a",
                                        "disk activity indicator.",
                                        "",
                                        "The LED will be turned on",
                                        "when reading from or writing",
                                        "to the storage device.",
                                        "",
                                        "This option has no effect on",
                                        "Wii U, since the Wii U does",
                                        "not have a drive slot LED.",
                                        NULL
                                };
                                return desc_led;
                        }
                        case NIN_CFG_BIT_LOG:
                                break;
                        case NIN_SETTINGS_MAX_PADS: {
                                static const char *desc_max_pads[] = {
                                        "Set the maximum number of",
                                        "native GameCube controller",
                                        "ports to use on Wii.",
                                        "",
                                        "This should usually be kept",
                                        "at 4 to enable all ports",
                                        "",
                                        "This option has no effect on",
                                        "Wii U and Wii Family Edition",
                                        "systems.",
                                        NULL
                                };
                                return desc_max_pads;
                        }
                        case NIN_SETTINGS_LANGUAGE: {
                                static const char *desc_language[] = {
                                        "Set the system language.",
                                        "",
                                        "This option is normally only",
                                        "found on PAL GameCubes, so",
                                        "it usually won't have an",
                                        "effect on NTSC games.",
                                        NULL
                                };
                                return desc_language;
                        }
                        case NIN_SETTINGS_VIDEO:
                        case NIN_SETTINGS_VIDEOMODE:
                                break;
                        case NIN_SETTINGS_MEMCARDBLOCKS: {
                                static const char *desc_memcard_blocks[] = {
                                        "Default size for new memory",
                                        "card images.",
                                        "",
                                        "NOTE: Sizes larger than 251",
                                        "blocks are known to cause",
                                        "issues.",
                                        NULL
                                };
                                return desc_memcard_blocks;
                        }
                        case NIN_SETTINGS_MEMCARDMULTI: {
                                static const char *desc_memcard_multi[] = {
                                        "Nintendont usually uses one",
                                        "emulated memory card image",
                                        "per game.",
                                        "",
                                        "Enabling MULTI switches this",
                                        "to use one memory card image",
                                        "for all USA and PAL games,",
                                        "and one image for all JPN",
                                        "games.",
                                        NULL
                                };
                                return desc_memcard_multi;
                        }
                        case NIN_SETTINGS_NATIVE_SI: {
                                static const char *desc_native_si[] = {
                                        "Native Control allows use of",
                                        "GBA link cables on original",
                                        "Wii systems.",
                                        "",
                                        "NOTE: Enabling Native Control",
                                        "will disable Bluetooth and",
                                        "USB HID controllers.",
                                        "",
                                        "This option is not available",
                                        "on Wii U.",
                                        NULL
                                };
                                return desc_native_si;
                        }
                        default:
                                break;
                }
        } else
        {
                switch (ctx->settings.posX)
                {
                        case 3: {
                                static const char *desc_tri_arcade[] = {
                                        "Arcade Mode re-enables the",
                                        "coin slot functionality of",
                                        "Triforce games.",
                                        "",
                                        "To insert a coin, move the",
                                        "C stick in any direction.",
                                        NULL
                                };
                                return desc_tri_arcade;
                        }
                        case 4: {
                                static const char *desc_cc_rumble[] = {
                                        "Enable rumble on Wii Remotes",
                                        "when using the Wii Classic",
                                        "Controller or Wii Classic",
                                        "Controller Pro.",
                                        NULL
                                };
                                return desc_cc_rumble;
                        }
                        case 5: {
                                static const char *desc_skip_ipl[] = {
                                        "Skip loading the GameCube",
                                        "IPL, even if it's present",
                                        "on the storage device.",
                                        NULL
                                };
                                return desc_skip_ipl;
                        }
                        case 6: {
                                static const char *desc_skip_bba[] = {
                                        "Enable BBA Emulation in the",
                                        "following supported titles",
                                        "including all their regions:",
                                        "",
                                        "Mario Kart: Double Dash!!",
                                        "Kirby Air Ride",
                                        "1080 Avalanche",
                                        "PSO Episode 1&2",
                                        "PSO Episode III",
                                        "Homeland",
                                        NULL
                                };
                                return desc_skip_bba;
                        }
                        case 7: {
                                static const char *desc_skip_netprof[] = {
                                        "Force a Network Profile",
                                        "to use for BBA Emulation,",
                                        "this option only works on",
                                        "the original Wii because",
                                        "on Wii U the profiles are",
                                        "managed by the Wii U Menu.",
                                        "This means you can even",
                                        "use profiles that cannot",
                                        "connect to the internet.",
                                        NULL
                                };
                                return desc_skip_netprof;
                        }
                        default:
                                break;
                }
        }
        return NULL;
}

static bool UpdateSettingsMenu(MenuCtx *ctx)
{
    u32 ListLoopIndex;
    int i;
    int k;

        if(FPAD_X(0))
        {
                UpdateNintendont();
                ctx->redraw = 1;
        }

        if (FPAD_Down_Repeat(ctx))
        {
                if (ctx->settings.settingPart == 0) {
                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+30, SettingY(ctx->settings.posX), " " );
                } else {
                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+300, SettingY(ctx->settings.posX), " " );
                }
                ctx->settings.posX++;
                if (ctx->settings.settingPart == 0)
                {
                        if (((ncfg->VideoMode & NIN_VID_FORCE) == 0) &&
                            (ctx->settings.posX == NIN_SETTINGS_VIDEOMODE))
                        {
                                ctx->settings.posX++;
                        }
                        if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) &&
                            (ctx->settings.posX == NIN_SETTINGS_MEMCARDBLOCKS))
                        {
                                ctx->settings.posX++;
                        }
                        if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) &&
                            (ctx->settings.posX == NIN_SETTINGS_MEMCARDMULTI))
                        {
                                ctx->settings.posX++;
                        }
                }
                if ((ctx->settings.settingPart == 0 && ctx->settings.posX >= NIN_SETTINGS_LAST) ||
                    (ctx->settings.settingPart == 1 && ctx->settings.posX >= 9))
                {
                        ctx->settings.posX = 0;
                        ctx->settings.settingPart ^= 1;
                }
                ctx->redraw = true;
        }
        else if (FPAD_Up_Repeat(ctx))
        {
                if (ctx->settings.settingPart == 0) {
                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+30, SettingY(ctx->settings.posX), " " );
                } else {
                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+300, SettingY(ctx->settings.posX), " " );
                }
                ctx->settings.posX--;
                if (ctx->settings.posX < 0)
                {
                        ctx->settings.settingPart ^= 1;
                        if (ctx->settings.settingPart == 0) {
                                ctx->settings.posX = NIN_SETTINGS_LAST - 1;
                        } else {
                                ctx->settings.posX = 8;
                        }
                }
                if (ctx->settings.settingPart == 0)
                {
                        if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) &&
                            (ctx->settings.posX == NIN_SETTINGS_MEMCARDMULTI))
                        {
                                ctx->settings.posX--;
                        }
                        if ((!(ncfg->Config & NIN_CFG_MEMCARDEMU)) &&
                            (ctx->settings.posX == NIN_SETTINGS_MEMCARDBLOCKS))
                        {
                                ctx->settings.posX--;
                        }
                        if (((ncfg->VideoMode & NIN_VID_FORCE) == 0) &&
                            (ctx->settings.posX == NIN_SETTINGS_VIDEOMODE))
                        {
                                ctx->settings.posX--;
                        }
                }
                ctx->redraw = true;
        }

        if (FPAD_Left_Repeat(ctx))
        {
                if (ctx->settings.settingPart == 1)
                {
                        ctx->saveSettings = true;
                        switch (ctx->settings.posX)
                        {
                                case 0:
                                        if (ncfg->VideoScale == 0) {
                                                ncfg->VideoScale = 120;
                                        } else {
                                                ncfg->VideoScale -= 2;
                                                if (ncfg->VideoScale < 40 || ncfg->VideoScale > 120) {
                                                        ncfg->VideoScale = 0;
                                                }
                                        }
                                        ReconfigVideo(rmode);
                                        ctx->redraw = true;
                                        break;
                                case 1:
                                        ncfg->VideoOffset--;
                                        if (ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20) {
                                                ncfg->VideoOffset = 20;
                                        }
                                        ReconfigVideo(rmode);
                                        ctx->redraw = true;
                                        break;
                                default:
                                        break;
                        }
                }
        }
        else if (FPAD_Right_Repeat(ctx))
        {
                if (ctx->settings.settingPart == 1)
                {
                        ctx->saveSettings = true;
                        switch (ctx->settings.posX)
                        {
                                case 0:
                                        if(ncfg->VideoScale == 0) {
                                                ncfg->VideoScale = 40;
                                        } else {
                                                ncfg->VideoScale += 2;
                                                if (ncfg->VideoScale < 40 || ncfg->VideoScale > 120) {
                                                        ncfg->VideoScale = 0;
                                                }
                                        }
                                        ReconfigVideo(rmode);
                                        ctx->redraw = true;
                                        break;
                                case 1:
                                        ncfg->VideoOffset++;
                                        if (ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20) {
                                                ncfg->VideoOffset = -20;
                                        }
                                        ReconfigVideo(rmode);
                                        ctx->redraw = true;
                                        break;
                                default:
                                        break;
                        }
                }
        }

        if( FPAD_OK(0) )
        {
                if (ctx->settings.settingPart == 0)
                {
                        ctx->saveSettings = true;
                        if (ctx->settings.posX < NIN_CFG_BIT_LAST)
                        {
                                // Standard boolean settings, including NIN_CFG_BIT_FORCE_PROG
                                if (ctx->settings.posX == NIN_CFG_BIT_USB) {
                                        ncfg->Config ^= NIN_CFG_WIIU_WIDE;
                                } else {
                                    ncfg->Config ^= (1 << ctx->settings.posX);
                                    // If NIN_CFG_FORCE_PROG was just turned ON
                                    if ((ctx->settings.posX == NIN_CFG_BIT_FORCE_PROG) && (ncfg->Config & NIN_CFG_FORCE_PROG)) {
                                        // Clear specific 240p/288p/576p mode flags from ncfg->VideoMode
                                        ncfg->VideoMode &= ~(NIN_VID_FORCE_NTSC_240P | NIN_VID_FORCE_PAL_288P | NIN_VID_FORCE_MPAL_240P | NIN_VID_FORCE_EURGB60_240P | NIN_VID_FORCE_PAL_576P);
                                        // Also clear the NIN_VID_PROG bit in ncfg->VideoMode, as NIN_CFG_FORCE_PROG handles the general 480p progressive
                                        ncfg->VideoMode &= ~NIN_VID_PROG;
                                    }
                                }
                        }
                        else switch (ctx->settings.posX)
                        {
                                case NIN_SETTINGS_MAX_PADS:
                                        ncfg->MaxPads++;
                                        if (ncfg->MaxPads > NIN_CFG_MAXPAD) {
                                                ncfg->MaxPads = 0;
                                        }
                                        break;
                                case NIN_SETTINGS_LANGUAGE:
                                        ncfg->Language++;
                                        if (ncfg->Language > NIN_LAN_DUTCH) {
                                                ncfg->Language = NIN_LAN_AUTO;
                                        }
                                        break;
                                case NIN_SETTINGS_VIDEO:
                                {
                                        u32 Video = (ncfg->VideoMode & NIN_VID_MASK);
                                        switch (Video)
                                        {
                                                case NIN_VID_AUTO:   Video = NIN_VID_FORCE; break;
                                                case NIN_VID_FORCE:  Video = NIN_VID_FORCE | NIN_VID_FORCE_DF; break;
                                                case NIN_VID_FORCE | NIN_VID_FORCE_DF: Video = NIN_VID_NONE; break;
                                                default:
                                                case NIN_VID_NONE:   Video = NIN_VID_AUTO; break;
                                        }
                                        ncfg->VideoMode &= ~NIN_VID_MASK;
                                        ncfg->VideoMode |= Video;
                                        break;
                                }
                                case NIN_SETTINGS_VIDEOMODE:
                                {
                                    u32 currentForceSpecificFlags = ncfg->VideoMode & NIN_VID_FORCE_MASK;
                                    int currentModeIndex = -1;
                                    for (i = 0; i < NUM_VIDEOMODE_STRINGS; i++) {
                                        if (currentForceSpecificFlags == videoModeFlagMap[i]) {
                                            currentModeIndex = i;
                                            break;
                                        }
                                    }
                                    currentModeIndex++;
                                    if (currentModeIndex >= NUM_VIDEOMODE_STRINGS) {
                                        currentModeIndex = 0;
                                    }
                                    ncfg->VideoMode &= ~(NIN_VID_FORCE_MASK | NIN_VID_PROG);
                                    ncfg->VideoMode |= videoModeFlagMap[currentModeIndex];

                                    u32 selectedFlag = videoModeFlagMap[currentModeIndex];
                                    if (selectedFlag & (NIN_VID_FORCE_NTSC_240P | NIN_VID_FORCE_PAL_288P | NIN_VID_FORCE_MPAL_240P | NIN_VID_FORCE_EURGB60_240P)) {
                                        ncfg->Config &= ~NIN_CFG_FORCE_PROG;
                                    }
                                    else if (selectedFlag == NIN_VID_FORCE_PAL_576P) {
                                        ncfg->VideoMode |= NIN_VID_PROG;
                                        ncfg->Config &= ~NIN_CFG_FORCE_PROG;
                                    }
                                    break;
                                }
                                case NIN_SETTINGS_MEMCARDBLOCKS:
                                        ncfg->MemCardBlocks++;
                                        if (ncfg->MemCardBlocks > MEM_CARD_MAX) {
                                                ncfg->MemCardBlocks = 0;
                                        }
                                        break;
                                case NIN_SETTINGS_MEMCARDMULTI:
                                        ncfg->Config ^= (NIN_CFG_MC_MULTI);
                                        break;
                                case NIN_SETTINGS_NATIVE_SI:
                                        ncfg->Config ^= (NIN_CFG_NATIVE_SI);
                                        break;
                                default:
                                        break;
                        }
                        if (!(ncfg->Config & NIN_CFG_MEMCARDEMU))
                        {
                                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDBLOCKS), "%29s", "");
                                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(NIN_SETTINGS_MEMCARDMULTI), "%29s", "");
                        }
                        ctx->redraw = true;
                }
                else if (ctx->settings.settingPart == 1)
                {
                        switch (ctx->settings.posX) {
                                case 2:
                                        ctx->saveSettings = true;
                                        ncfg->VideoMode ^= (NIN_VID_PATCH_PAL50);
                                        ctx->redraw = true;
                                        break;
                                case 3:
                                        ctx->saveSettings = true;
                                        ncfg->Config ^= (NIN_CFG_ARCADE_MODE);
                                        ctx->redraw = true;
                                        break;
                                case 4:
                                        ctx->saveSettings = true;
                                        ncfg->Config ^= (NIN_CFG_CC_RUMBLE);
                                        ctx->redraw = true;
                                        break;
                                case 5:
                                        ctx->saveSettings = true;
                                        ncfg->Config ^= (NIN_CFG_SKIP_IPL);
                                        ctx->redraw = true;
                                        break;
                                case 6:
                                        ctx->saveSettings = true;
                                        ncfg->Config ^= (NIN_CFG_BBA_EMU);
                                        ctx->redraw = true;
                                        break;
                                case 7:
                                        ctx->saveSettings = true;
                                        ncfg->NetworkProfile++;
                                        ncfg->NetworkProfile &= 3;
                                        ctx->redraw = true;
                                        break;
                                case 8:
                                        ctx->saveSettings = true;
                                        ncfg->WiiUGamepadSlot++;
                                        if (ncfg->WiiUGamepadSlot > NIN_CFG_MAXPAD) {
                                                ncfg->WiiUGamepadSlot = 0;
                                        }
                                        ctx->redraw = true;
                                        break;
                                default:
                                        break;
                        }
                }
        }

        if (ctx->redraw)
        {
                for (ListLoopIndex = 0; ListLoopIndex < NIN_CFG_BIT_LAST; ListLoopIndex++)
                {
                        if (ListLoopIndex == NIN_CFG_BIT_USB) {
                                PrintFormat(MENU_SIZE, (IsWiiU() ? BLACK : DARK_GRAY), MENU_POS_X+50, SettingY(ListLoopIndex),
                                            "%-18s:%s", OptionStrings[ListLoopIndex], (ncfg->Config & (NIN_CFG_WIIU_WIDE)) ? "On " : "Off");
                        } else {
                                u32 item_color = BLACK;
                                if (IsWiiU() &&
                                    (ListLoopIndex == NIN_CFG_BIT_DEBUGGER ||
                                     ListLoopIndex == NIN_CFG_BIT_DEBUGWAIT ||
                                     ListLoopIndex == NIN_CFG_BIT_LED))
                                {
                                        item_color = DARK_GRAY;
                                }
                                PrintFormat(MENU_SIZE, item_color, MENU_POS_X+50, SettingY(ListLoopIndex),
                                            "%-18s:%s", OptionStrings[ListLoopIndex], (ncfg->Config & (1 << ListLoopIndex)) ? "On " : "Off" );
                        }
                }
                PrintFormat(MENU_SIZE, (!IsWiiU() ? BLACK : DARK_GRAY), MENU_POS_X+50, SettingY(ListLoopIndex),
                            "%-18s:%d", OptionStrings[ListLoopIndex], (ncfg->MaxPads));
                ListLoopIndex++;
                u32 LanIndex = ncfg->Language;
                if (LanIndex < NIN_LAN_FIRST || LanIndex >= NIN_LAN_LAST) {
                        LanIndex = NIN_LAN_LAST;
                }
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),
                            "%-18s:%-4s", OptionStrings[ListLoopIndex], LanguageStrings[LanIndex] );
                ListLoopIndex++;
                u32 VideoModeIndex_display;
                u32 VideoModeVal = ncfg->VideoMode & NIN_VID_MASK;
                switch (VideoModeVal)
                {
                        case NIN_VID_AUTO:   VideoModeIndex_display = NIN_VID_INDEX_AUTO; break;
                        case NIN_VID_FORCE:  VideoModeIndex_display = NIN_VID_INDEX_FORCE; break;
                        case NIN_VID_FORCE | NIN_VID_FORCE_DF: VideoModeIndex_display = NIN_VID_INDEX_FORCE_DF; break;
                        case NIN_VID_NONE:   VideoModeIndex_display = NIN_VID_INDEX_NONE; break;
                        default: ncfg->VideoMode &= ~NIN_VID_MASK; ncfg->VideoMode |= NIN_VID_AUTO; VideoModeIndex_display = NIN_VID_INDEX_AUTO; break;
                }
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),
                            "%-18s:%-18s", OptionStrings[ListLoopIndex], VideoStrings[VideoModeIndex_display] );
                ListLoopIndex++;
                if( ncfg->VideoMode & NIN_VID_FORCE )
                {
                    u32 currentForceSpecificFlags = ncfg->VideoMode & NIN_VID_FORCE_MASK;
                    int displayStringIndex = -1;
                    for(i=0; i < NUM_VIDEOMODE_STRINGS; ++i) {
                        if (currentForceSpecificFlags == videoModeFlagMap[i]) {
                            displayStringIndex = i;
                            break;
                        }
                    }
                    if (displayStringIndex == -1) {
                         displayStringIndex = 2;
                         if (!(currentForceSpecificFlags & videoModeFlagMap[displayStringIndex])) {
                            displayStringIndex = 0;
                         }
                    }
                    PrintFormat(MENU_SIZE, BLACK, MENU_POS_X+50, SettingY(ListLoopIndex),
                                "%-18s:%-10s", OptionStrings[ListLoopIndex], VideoModeStrings[displayStringIndex] );
                } else {
                     PrintFormat(MENU_SIZE, DARK_GRAY, MENU_POS_X+50, SettingY(ListLoopIndex),
                                "%-18s:%-10s", OptionStrings[ListLoopIndex], "---" );
                }
                ListLoopIndex++;
                if ((ncfg->Config & NIN_CFG_MEMCARDEMU) == NIN_CFG_MEMCARDEMU)
                {
                        u32 MemCardBlocksVal = ncfg->MemCardBlocks;
                        if (MemCardBlocksVal > MEM_CARD_MAX) {
                                MemCardBlocksVal = 0;
                        }
                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex),
                                    "%-18s:%-4d%s", OptionStrings[ListLoopIndex], MEM_CARD_BLOCKS(MemCardBlocksVal), MemCardBlocksVal > 2 ? "Unstable" : "");
                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 50, SettingY(ListLoopIndex+1),
                                    "%-18s:%-4s", OptionStrings[ListLoopIndex+1], (ncfg->Config & (NIN_CFG_MC_MULTI)) ? "On " : "Off");
                }
                ListLoopIndex+=2;
                PrintFormat(MENU_SIZE, (IsWiiU() ? DARK_GRAY : BLACK), MENU_POS_X + 50, SettingY(ListLoopIndex),
                            "%-18s:%-4s", OptionStrings[ListLoopIndex],
                            (ncfg->Config & (NIN_CFG_NATIVE_SI)) ? "On " : "Off");
                ListLoopIndex++;
                ListLoopIndex = 0;
                char vidWidth[10];
                if (ncfg->VideoScale < 40 || ncfg->VideoScale > 120) {
                        ncfg->VideoScale = 0;
                        snprintf(vidWidth, sizeof(vidWidth), "Auto");
                } else {
                        snprintf(vidWidth, sizeof(vidWidth), "%i", ncfg->VideoScale + 600);
                }
                char vidOffset[10];
                if (ncfg->VideoOffset < -20 || ncfg->VideoOffset > 20) {
                        ncfg->VideoOffset = 0;
                }
                snprintf(vidOffset, sizeof(vidOffset), "%i", ncfg->VideoOffset);
                char netProfile[5];
                ncfg->NetworkProfile &= 3;
                if(ncfg->NetworkProfile == 0)
                        snprintf(netProfile, sizeof(netProfile), "Auto");
                else
                        snprintf(netProfile, sizeof(netProfile), "%i", ncfg->NetworkProfile);

                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "Video Width", vidWidth);
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "Screen Position", vidOffset);
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "Patch PAL50", (ncfg->VideoMode & (NIN_VID_PATCH_PAL50)) ? "On " : "Off");
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "TRI Arcade Mode", (ncfg->Config & (NIN_CFG_ARCADE_MODE)) ? "On " : "Off");
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "Wiimote CC Rumble", (ncfg->Config & (NIN_CFG_CC_RUMBLE)) ? "On " : "Off");
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "Skip IPL", (ncfg->Config & (NIN_CFG_SKIP_IPL)) ? "Yes" : "No ");
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "BBA Emulation", (ncfg->Config & (NIN_CFG_BBA_EMU)) ? "On" : "Off");
                ListLoopIndex++;
                PrintFormat(MENU_SIZE, (IsWiiU() || !(ncfg->Config & (NIN_CFG_BBA_EMU))) ? DARK_GRAY : BLACK,
                                MENU_POS_X + 320, SettingY(ListLoopIndex),
                            "%-18s:%-4s", "Network Profile", netProfile);
                ListLoopIndex++;
                if (ncfg->WiiUGamepadSlot < NIN_CFG_MAXPAD) {
                        PrintFormat(MENU_SIZE, (IsWiiU() ? BLACK : DARK_GRAY), MENU_POS_X+320, SettingY(ListLoopIndex),
                                        "%-18s:%d", "WiiU Gamepad Slot", (ncfg->WiiUGamepadSlot + 1));
                } else {
                        PrintFormat(MENU_SIZE, (IsWiiU() ? BLACK : DARK_GRAY), MENU_POS_X+320, SettingY(ListLoopIndex),
                        "%-18s:%-4s", "WiiU Gamepad Slot", "None");
                }
                ListLoopIndex++;
                u32 cursor_color = BLACK;
                if (ctx->settings.settingPart == 0) {
                        if ((!IsWiiU() && ctx->settings.posX == NIN_CFG_BIT_USB) ||
                             (IsWiiU() && ctx->settings.posX == NIN_SETTINGS_NATIVE_SI) ||
                             (IsWiiU() && ctx->settings.posX == NIN_SETTINGS_MAX_PADS) ||
                             (IsWiiU() && ctx->settings.posX == NIN_CFG_BIT_DEBUGGER) ||
                             (IsWiiU() && ctx->settings.posX == NIN_CFG_BIT_DEBUGWAIT) ||
                             (IsWiiU() && ctx->settings.posX == NIN_CFG_BIT_LED)
                             )
                        {
                                cursor_color = DARK_GRAY;
                        }
                        PrintFormat(MENU_SIZE, cursor_color, MENU_POS_X + 30, SettingY(ctx->settings.posX), ARROW_RIGHT);
                } else {
                        if((IsWiiU() || !(ncfg->Config & (NIN_CFG_BBA_EMU))) && ctx->settings.posX == 7)
                                cursor_color = DARK_GRAY;
                        if(IsWiiU() && ctx->settings.posX == 8)
                                cursor_color = BLACK;
                        else if (!IsWiiU() && ctx->settings.posX == 8)
                                cursor_color = DARK_GRAY;
                        PrintFormat(MENU_SIZE, cursor_color, MENU_POS_X + 300, SettingY(ctx->settings.posX), ARROW_RIGHT);
                }
                const char *const *desc = GetSettingsDescription(ctx);
                if (desc != NULL)
                {
                        int line_num = 9;
                        for (k=0; k<8; ++k) {
                             PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 300, SettingY(line_num+k), "%30s", "");
                        }
                        do {
                                if (**desc != 0)
                                {
                                        PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 300, SettingY(line_num), *desc);
                                }
                                line_num++;
                        } while (*(++desc) != NULL && line_num < 9+8);
                } else {
                        for (k=0; k<8; ++k) {
                             PrintFormat(MENU_SIZE, BLACK, MENU_POS_X + 300, SettingY(9+k), "%30s", "");
                        }
                }
        }
        return false;
}

static int SelectGame(void)
{
        ShowLoadingScreen();
        u32 gamecount = 0;
        gameinfo gi[MAX_GAMES];
        u32 i;
        devState = LoadGameList(&gi[0], MAX_GAMES, &gamecount);
        switch (devState)
        {
                case DEV_OK: break;
                case DEV_NO_GAMES: gprintf("WARNING: %s:/games/ was not found.\n", GetRootDevice()); break;
                case DEV_NO_TITLES: gprintf("WARNING: %s:/games/ contains no GC titles.\n", GetRootDevice()); break;
                case DEV_NO_OPEN: default:
                {
                        const char *s_devType = (UseSD ? "SD" : "USB");
                        gprintf("No %s FAT device found.\n", s_devType);
                        break;
                }
        }
        MenuCtx ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.menuMode = 0;
        ctx.redraw = true;
        ctx.selected = false;
        ctx.saveSettings = false;
        ctx.games.listMax = gamecount;
        if (ctx.games.listMax > 15) {
                ctx.games.listMax = 15;
        }
        ctx.games.gi = gi;
        ctx.games.gamecount = gamecount;
        for (i = 0; i < gamecount; ++i)
        {
                if (strcasecmp(strchr(gi[i].Path,':')+1, ncfg->GamePath) == 0)
                {
                        if (i >= ctx.games.listMax) {
                                ctx.games.posX    = ctx.games.listMax - 1;
                                ctx.games.scrollX = i - ctx.games.listMax + 1;
                        } else {
                                ctx.games.posX = i;
                        }
                        break;
                }
        }

        while(1)
        {
                VIDEO_WaitVSync();
                FPAD_Update();
                if(Shutdown)
                        LoaderShutdown();
                if( FPAD_Start(0) )
                {
                        ctx.selected = false;
                        break;
                }
                if( FPAD_Cancel(0) )
                {
                        ctx.menuMode = !ctx.menuMode;
                        memset(&ctx.held, 0, sizeof(ctx.held));
                        if (ctx.menuMode == 1)
                        {
                                ctx.settings.posX = 0;
                                ctx.settings.settingPart = 0;
                        }
                        ctx.redraw = 1;
                }
                bool ret = false;
                if (ctx.menuMode == 0) {
                        ret = UpdateGameSelectMenu(&ctx);
                } else {
                        ret = UpdateSettingsMenu(&ctx);
                }
                if (ret)
                {
                        break;
                }
                if (ctx.redraw)
                {
                        PrintInfo();
                        if (ctx.menuMode == 0)
                        {
                                PrintButtonActions("Go Back", NULL, "Settings", NULL);
                                u32 color = ((ctx.games.canBeBooted) ? BLACK : DARK_GRAY);
                                PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : Select");
                                color = ((ctx.games.canShowInfo) ? BLACK : DARK_GRAY);
                                PrintFormat(DEFAULT_SIZE, color, MENU_POS_X + 430, MENU_POS_Y + 20*3, "X/1 : Game Info");
                        }
                        else
                        {
                                PrintButtonActions("Go Back", "Select", "Settings", "Update");
                        }
                        if (ctx.menuMode == 0 ||
                            (ctx.menuMode == 1 && devState == DEV_OK))
                        {
                                PrintDevInfo();
                        }
                        GRRLIB_Render();
                        Screenshot();
                        ClearScreen();
                        ctx.redraw = false;
                }
        }
        if(ctx.games.canBeBooted && ctx.games.gamecount > 0 && (ctx.games.posX + ctx.games.scrollX < ctx.games.gamecount))
        {
                u32 SelectedGame = ctx.games.posX + ctx.games.scrollX;
                const char* StartChar = gi[SelectedGame].Path + 3;
                if (StartChar[0] == ':') {
                        StartChar++;
                }
                strncpy(ncfg->GamePath, StartChar, sizeof(ncfg->GamePath)-1);
                ncfg->GamePath[sizeof(ncfg->GamePath)-1] = 0;
                memcpy(&(ncfg->GameID), gi[SelectedGame].ID, 4);
                DCFlushRange((void*)ncfg, sizeof(NIN_CFG));
        }
        for (i = 0; i < gamecount; ++i)
        {
                if (gi[i].Flags & GIFLAG_NAME_ALLOC)
                        free(gi[i].Name);
                if (gi[i].Path)
                    free(gi[i].Path);
        }
        if (!ctx.selected)
        {
                return 0;
        }
        return (ctx.saveSettings ? 3 : 1);
}

bool SelectDevAndGame(void)
{
        bool SaveSettings = false;
        bool redraw = true;
        while (1)
        {
                VIDEO_WaitVSync();
                FPAD_Update();
                if(Shutdown)
                        LoaderShutdown();
                if (redraw)
                {
                        UseSD = (ncfg->Config & NIN_CFG_USB) == 0;
                        PrintInfo();
                        PrintButtonActions("Exit", "Select", NULL, NULL);
                        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 6, UseSD ? ARROW_LEFT : "");
                        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 53 * 6 - 8, MENU_POS_Y + 20 * 7, UseSD ? "" : ARROW_LEFT);
                        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 6, " SD  ");
                        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 47 * 6 - 8, MENU_POS_Y + 20 * 7, "USB  ");
                        redraw = false;
                        GRRLIB_Render();
                        ClearScreen();
                }

                if (FPAD_OK(0))
                {
                        int ret = SelectGame();
                        if (ret & 2) SaveSettings = true;
                        if (ret & 1) break;
                        redraw = true;
                }
                else if (FPAD_Start(0))
                {
                        ShowMessageScreenAndExit("Returning to loader...", 0);
                }
                else if (FPAD_Down(0))
                {
                        ncfg->Config = ncfg->Config | NIN_CFG_USB;
                        redraw = true;
                }
                else if (FPAD_Up(0))
                {
                        ncfg->Config = ncfg->Config & ~NIN_CFG_USB;
                        redraw = true;
                }
        }
        return SaveSettings;
}

void ShowMessageScreen(const char *msg)
{
        const int len = strlen(msg);
        const int x = (640 - (len*10)) / 2;

        ClearScreen();
        PrintInfo();
        PrintFormat(DEFAULT_SIZE, BLACK, x, 232, "%s", msg);
        GRRLIB_Render();
        ClearScreen();
}

void ShowMessageScreenAndExit(const char *msg, int ret)
{
        const int len = strlen(msg);
        const int x = (640 - (len*10)) / 2;
        const u32 color = (ret == 0 ? BLACK : MAROON);

        ClearScreen();
        PrintInfo();
        PrintFormat(DEFAULT_SIZE, color, x, 232, "%s", msg);
        ExitToLoader(ret);
}

void PrintInfo(void)
{
        const char *consoleType = (isWiiVC ? (IsWiiUFastCPU() ? "WiiVC 5x CPU" : "Wii VC") : (IsWiiUFastCPU() ? "WiiU 5x CPU" : (IsWiiU() ? "Wii U" : "Wii")));
#ifdef NIN_SPECIAL_VERSION
        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*0, "Nintendont Loader v%u.%u" NIN_SPECIAL_VERSION " (%s)",
                    NIN_VERSION>>16, NIN_VERSION&0xFFFF, consoleType);
#else
        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*0, "Nintendont Loader v%u.%u (%s)",
                    NIN_VERSION>>16, NIN_VERSION&0xFFFF, consoleType);
#endif
        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*1, "Built : " __DATE__ " " __TIME__);
        PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*2, "Firmware: %u.%u.%u",
                    *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143);
}

void PrintButtonActions(const char *btn_home, const char *btn_a, const char *btn_b, const char *btn_x1)
{
        if (btn_home) {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*0, "Home: %s", btn_home);
        }
        if (btn_a) {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*1, "A   : %s", btn_a);
        }
        if (btn_b) {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*2, "B   : %s", btn_b);
        }
        if (btn_x1) {
                PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 430, MENU_POS_Y + 20*3, "X/1 : %s", btn_x1);
        }
}

static void PrintDevInfo(void)
{
        const char *s_devType = (UseSD ? "SD" : "USB");
        switch (devState) {
                case DEV_NO_OPEN:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
                                "WARNING: %s FAT device could not be opened.", s_devType);
                        break;
                case DEV_NO_GAMES:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
                                "WARNING: %s:/games/ was not found.", GetRootDevice());
                        break;
                case DEV_NO_TITLES:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4,
                                "WARNING: %s:/games/ contains no GC titles.", GetRootDevice());
                        break;
                default:
                        break;
        }
}

void ReconfigVideo(GXRModeObj *vidmode_arg)
{
    if (!vidmode_arg) return;

    GXRModeObj temp_vmode = *vidmode_arg;

    if(ncfg->VideoScale >= 40 && ncfg->VideoScale <= 120)
        temp_vmode.viWidth = ncfg->VideoScale + 600;
    else
        temp_vmode.viWidth = 640;

    temp_vmode.viXOrigin = (720 - temp_vmode.viWidth) / 2;

    if(ncfg->VideoOffset >= -20 && ncfg->VideoOffset <= 20)
    {
        s32 new_x_origin = temp_vmode.viXOrigin + ncfg->VideoOffset;
        if(new_x_origin < 0)
            temp_vmode.viXOrigin = 0;
        else if(new_x_origin > 80)
            temp_vmode.viXOrigin = 80;
        else
            temp_vmode.viXOrigin = new_x_origin;
    }
    VIDEO_Configure(&temp_vmode);
}

void PrintLoadKernelError(LoadKernelError_t iosErr, int err)
{
        ClearScreen();
        PrintInfo();
        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*4, "Failed to load IOS58 from NAND:");

        switch (iosErr)
        {
                case LKERR_UNKNOWN:
                default:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "LoadKernel() error %d occurred, returning %d.", iosErr, err);
                        break;
                case LKERR_ES_GetStoredTMDSize:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "ES_GetStoredTMDSize() returned %d.", err);
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "This usually means IOS58 is not installed.");
                        if (IsWiiU())
                        {
                                PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "WARNING: On Wii U, a missing IOS58 may indicate");
                                PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "something is seriously wrong with the vWii setup.");
                        }
                        else
                        {
                                PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*9, "Please update to Wii System 4.3 and try running");
                                PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Nintendont again.");
                        }
                        break;
                case LKERR_TMD_malloc:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "Unable to allocate memory for the IOS58 TMD.");
                        break;
                case LKERR_ES_GetStoredTMD:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "ES_GetStoredTMD() returned %d.", err);
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
                        break;
                case LKERR_IOS_Open_shared1_content_map:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Open(\"/shared1/content.map\") returned %d.", err);
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "This usually means Nintendont was not started with");
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*8, "AHB access permissions.");
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*10, "Please ensure that meta.xml is present in your Nintendont");
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*11, "application directory and that it contains a line");
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*12, "with the tag <ahb_access/> .");
                        break;
                case LKERR_IOS_Open_IOS58_kernel:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Open(IOS58 kernel) returned %d.", err);
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
                        break;
                case LKERR_IOS_Read_IOS58_kernel:
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*5, "IOS_Read(IOS58 kernel) returned %d.", err);
                        PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*7, "WARNING: IOS58 may be corrupted.");
                        break;
        }
}
