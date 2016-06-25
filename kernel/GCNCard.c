// Nintendont (kernel): GameCube Memory Card functions.
// Used by EXI.c.

#include "GCNCard.h"
#include "Config.h"
#include "debug.h"
#include "ff_utf8.h"

// Triforce variables.
extern vu32 TRIGame;

// Memory Card context.
static u8 *const GCNCard_base = (u8*)(0x11000000);

typedef struct _GCNCard_ctx {
	char filename[0x20];    // Memory Card filename.
	u8 *base;               // Base address.
	u32 size;               // Size, in bytes.
	u32 code;               // Memory card "code".

	// BlockOffLow starts from 0xA000; does not include "system" blocks.
	// For system blocks, check 'changed_system'.
	bool changed;		// True if the card has been modified at all.
				// (NOTE: Reset after calling GCNCard_CheckChanges().)
	bool changed_system;	// True if the system area (first 5 blocks)
				// has been modified. These blocks are NOT
				// included in BlockOffLow / BlockOffHigh.

	// NOTE: BlockOff is in bytes, not blocks.
	u32 BlockOff;           // Current offset.
	u32 BlockOffLow;        // Low address of last modification.
	u32 BlockOffHigh;       // High address of last modification.
	u32 CARDWriteCount;     // Write count. (TODO: Is this used anywhere?)
} GCNCard_ctx;
#ifdef GCNCARD_ENABLE_SLOT_B
static GCNCard_ctx memCard[2] __attribute__((aligned(32)));
#else /* !GCNCARD_ENABLE_SLOT_B */
static GCNCard_ctx memCard[1] __attribute__((aligned(32)));
#endif /* GCNCARD_ENABLE_SLOT_B */

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static void GCNCard_InitCtx(GCNCard_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->BlockOffLow = 0xFFFFFFFF;
}

/**
 * Is a memory card enabled?
 * @param slot Slot number. (0 == Slot A, 1 == Slot B)
 * @return 0 if disabled; 1 if enabled.
 */
inline u32 GCNCard_IsEnabled(int slot)
{
	if (slot < 0 || slot > ARRAY_SIZE(memCard))
		return 0;

	// Card is enabled if it's larger than 0 bytes.
	return (memCard[slot].size > 0);
}

/**
 * Load a memory card from disk.
 * @param slot Slot number. (0 == Slot A, 1 == Slot B)
 * NOTE: Slot B is not valid on Triforce.
 * @return 0 on success; non-zero on error.
 */
int GCNCard_Load(int slot)
{
	switch (slot)
	{
		case 0:
			// Slot A
			break;
#ifdef GCNCARD_ENABLE_SLOT_B
		case 1:
			// Slot B (not valid on Triforce)
			if (TRIGame != 0)
				return -1;
			break;
#endif /* GCNCARD_ENABLE_SLOT_B */
		default:
			// Invalid slot.
			return -2;
	}

	// Get the Game ID.
	const u32 GameID = ConfigGetGameID();

	// Set up the Memory Card context.
	GCNCard_ctx *const ctx = &memCard[slot];
	GCNCard_InitCtx(ctx);
	memcpy(ctx->filename, "/saves/", 7);
	char *fname_ptr = &ctx->filename[7];
	if (ConfigGetConfig(NIN_CFG_MC_MULTI))
	{
		// "Multi" mode enabled. (one card for all saves, per region)
		memcpy(fname_ptr, "ninmem", 6);
		fname_ptr += 6;

		if (BI2region == BI2_REGION_JAPAN ||
		    BI2region == BI2_REGION_SOUTH_KOREA)
		{
			// JPN game. Append a 'j'.
			*fname_ptr++ = 'j';
		}

#ifdef GCNCARD_ENABLE_SLOT_B
		if (slot)
		{
			// Slot B. Append a 'b'.
			*fname_ptr++ = 'b';
		}
#endif /* GCNCARD_ENABLE_SLOT_B */

		// Append the file extension. (with NULL terminator)
		memcpy(fname_ptr, ".raw", 5);
	}
	else
	{
		// Single mode. One card per game.
		memcpy(fname_ptr, &GameID, 4);
		fname_ptr += 4;

#ifdef GCNCARD_ENABLE_SLOT_B
		if (slot)
		{
			// Slot B. Append "_B".
			*(fname_ptr+0) = '_';
			*(fname_ptr+1) = 'B';
			fname_ptr += 2;
		}
#endif /* GCNCARD_ENABLE_SLOT_B */

		// Append the file extension. (with NULL terminator)
		memcpy(fname_ptr, ".raw", 5);
	}

	sync_after_write(ctx->filename, sizeof(ctx->filename));

	dbgprintf("EXI: Trying to open %s\r\n", ctx->filename);
	FIL fd;
	int ret = f_open_char(&fd, ctx->filename, FA_READ|FA_OPEN_EXISTING);
	if (ret != FR_OK || fd.obj.objsize == 0)
	{
#ifdef DEBUG_EXI
		dbgprintf("EXI: Slot %c: Failed to open %s: %u\r\n", (slot+'A'), ctx->filename, ret );
#endif
#ifdef GCNCARD_ENABLE_SLOT_B
		if (slot == 0)
		{
			// Slot A failure is fatal.
			Shutdown();
		}

		// Slot B failure will simply disable Slot B.
		dbgprintf("EXI: Slot %c has been disabled.\r\n", (slot+'A'));
		return -3;
#else /* !GCNCARD_ENABLE_SLOT_B */
		// Slot A failure is fatal.
		Shutdown();
#endif /* GCNCARD_ENABLE_SLOT_B */
	}

#ifdef DEBUG_EXI
	dbgprintf("EXI: Loading memory card for Slot %c...", (slot+'A'));
#endif

	// Check if the card filesize is valid.
	u32 FindBlocks = 0;
	for (FindBlocks = 0; FindBlocks <= MEM_CARD_MAX; FindBlocks++)
	{
		if (MEM_CARD_SIZE(FindBlocks) == fd.obj.objsize)
			break;
	}
	if (FindBlocks > MEM_CARD_MAX)
	{
		dbgprintf("EXI: Slot %c unexpected size %s: %u\r\n",
				(slot+'A'), ctx->filename, fd.obj.objsize);
#ifdef GCNCARD_ENABLE_SLOT_B
		if (slot == 0)
		{
			// Slot A failure is fatal.
			Shutdown();
		}

		// Slot B failure will simply disable Slot B.
		dbgprintf("EXI: Slot %c has been disabled.\r\n", (slot+'A'));
		f_close(&fd);
		return -4;
#else /* !GCNCARD_ENABLE_SLOT_B */
		// Slot A failure is fatal.
		Shutdown();
#endif /* GCNCARD_ENABLE_SLOT_B */
	}

#if GCNCARD_ENABLE_SLOT_B
	if (slot == 0)
	{
		// Slot A starts at GCNCard_base.
		ctx->base = GCNCard_base;
		// Set the memory card size for Slot A only.
		ConfigSetMemcardBlocks(FindBlocks);
	}
	else
	{
		// Slot B starts immediately after Slot A.
		// Make sure both cards fit within 16 MB.
		if (memCard[0].size + fd.obj.objsize > (16*1024*1024))
		{
			// Not enough memory for both cards.
			// Disable Slot B.
			dbgprintf("EXI: Slot A is %u MB; not enough space for Slot %c, which is %u MB.\r\n",
					"EXI: Slot %c has been disabled.\r\n",
					memCard[0].size / 1024 / 1024, (slot+'A'),
					fd.obj.objsize / 1024 / 1024, (slot+'A'));
			f_close(&fd);
			return -4;
		}
		ctx->base = memCard[0].base + memCard[0].size;
	}
#else /* !GCNCARD_ENABLE_SLOT_B */
	// Slot A starts at GCNCard_base.
	ctx->base = GCNCard_base;
	// Set the memory card size for Slot A only.
	ConfigSetMemcardBlocks(FindBlocks);
#endif /* GCNCARD_ENABLE_SLOT_B */

	// Size and "code".
	ctx->size = fd.obj.objsize;
	ctx->code = MEM_CARD_CODE(FindBlocks);

	// Read the memory card contents into RAM.
	UINT read;
	f_lseek(&fd, 0);
	f_read(&fd, ctx->base, ctx->size, &read);
	f_close(&fd);

	// Reset the low/high offsets to indicate that everything was just loaded.
	ctx->BlockOffLow = 0xFFFFFFFF;
	ctx->BlockOffHigh = 0x00000000;

#ifdef DEBUG_EXI
	dbgprintf("EXI: Loaded Slot %c memory card size %u\r\n", (slot+'A'), ctx->size);
#endif

	// Synchronize the memory card data.
	sync_after_write(ctx->base, ctx->size);

#ifdef GCNCARD_ENABLE_SLOT_B
	if (slot == 1)
	{
		// Slot B card image loaded successfully.
		ncfg->Config |= NIN_CFG_MC_SLOTB;
	}
#endif /* GCNCARD_ENABLE_SLOT_B */

	return 0;
}

/**
* Get the total size of the loaded memory cards.
* @return Total size, in bytes.
*/
u32 GCNCard_GetTotalSize(void)
{
#ifdef GCNCARD_ENABLE_SLOT_B
	return (memCard[0].size + memCard[1].size);
#else /* !GCNCARD_ENABLE_SLOT_B */
	return memCard[0].size;
#endif /* GCNCARD_ENABLE_SLOT_B */
}

/**
* Check if the memory cards have changed.
* @return True if either memory card has changed; false if not.
*/
bool GCNCard_CheckChanges(void)
{
	int slot;
	bool ret = false;
	for (slot = 0; slot < ARRAY_SIZE(memCard); slot++)
	{
		// CHeck if the memory card is dirty in general.
		if (memCard[slot].changed)
		{
			memCard[slot].changed = false;
			ret = true;
		}
	}
	return ret;
}

/**
* Save the memory card(s).
*/
void GCNCard_Save(void)
{
	if (TRIGame)
	{
		// Triforce doesn't use the standard EXI CARD interface.
		return;
	}

	int slot;
	for (slot = 0; slot < ARRAY_SIZE(memCard); slot++)
	{
		if (!GCNCard_IsEnabled(slot))
		{
			// Card isn't initialized.
			continue;
		}

		// Does this card have any unsaved changes?
		GCNCard_ctx *const ctx = &memCard[slot];
		if (ctx->changed_system ||
		    ctx->BlockOffLow < ctx->BlockOffHigh)
		{
//#ifdef DEBUG_EXI
			//dbgprintf("EXI: Saving memory card in Slot %c...", (slot+'A'));
//#endif
			FIL fd;
			int ret = f_open_char(&fd, ctx->filename, FA_WRITE|FA_OPEN_EXISTING);
			if (ret == FR_OK)
			{
				UINT wrote;
				sync_before_read(ctx->base, ctx->size);

				// Save the system area, if necessary.
				if (ctx->changed_system)
				{
					f_lseek(&fd, 0);
					f_write(&fd, ctx->base, 0xA000, &wrote);
				}

				// Save the general area, if necessary.
				if (ctx->BlockOffLow < ctx->BlockOffHigh)
				{
					f_lseek(&fd, ctx->BlockOffLow);
					f_write(&fd, &ctx->base[ctx->BlockOffLow],
						(ctx->BlockOffHigh - ctx->BlockOffLow), &wrote);
				}

				f_close(&fd);
//#ifdef DEBUG_EXI
				//dbgprintf("Done!\r\n");
//#endif
			}
			else
			{
				dbgprintf("\r\nEXI: Unable to open Slot %c memory card file: %u\r\n", (slot+'A'), ret);
			}

			// Reset the low/high offsets to indicate that everything has been saved.
			ctx->BlockOffLow = 0xFFFFFFFF;
			ctx->BlockOffHigh = 0x00000000;
			ctx->changed_system = false;
		}
	}
}

/** Functions used by EXIDeviceMemoryCard(). **/

void GCNCard_ClearWriteCount(int slot)
{
	if (!GCNCard_IsEnabled(slot))
		return;

	memCard[slot].CARDWriteCount = 0;
}

/**
 * Set the current block offset.
 * @param slot Slot number.
 * @param data Block offset from the EXI command.
 */
void GCNCard_SetBlockOffset(int slot, u32 data)
{
	if (!GCNCard_IsEnabled(slot))
		return;

	u32 BlockOff = ((data>>16)&0xFF)  << 17;
	BlockOff    |= ((data>> 8)&0xFF)  << 9;
	BlockOff    |= ((data&0xFF)  &3)  << 7;
	memCard[slot].BlockOff = BlockOff;
}

/**
 * Write data to the card using the current block offset.
 * @param slot Slot number.
 * @param data Data to write.
 * @param length Length of data to write, in bytes.
 */
void GCNCard_Write(int slot, const void *data, u32 length)
{
	if (!GCNCard_IsEnabled(slot))
		return;
	GCNCard_ctx *const ctx = &memCard[slot];
	ctx->changed = true;

	// Is this update entirely within the "system area"?
	if (ctx->BlockOff < 0xA000 && ctx->BlockOff + length < 0xA000)
	{
		// This update is entirely within the "system area".
		// Only set the flag; don't set block offsets.
		ctx->changed_system = true;
	}
	else
	{
		// Update the block offsets for saving.
		if (ctx->BlockOff < ctx->BlockOffLow)
			ctx->BlockOffLow = ctx->BlockOff;
		if (ctx->BlockOff + length > ctx->BlockOffHigh)
			ctx->BlockOffHigh = ctx->BlockOff + length;
		ctx->changed = true;

		if (ctx->BlockOff < 0xA000)
		{
			// System area as well as general area.
			ctx->changed_system = true;
		}
		if (ctx->BlockOffLow < 0xA000)
		{
			// BlockOffLow shouldn't be less than 0xA000.
			// Otherwise, we end up with double writing.
			// (Not a problem; just wastes time.)
			ctx->BlockOffLow = 0xA000;
			ctx->changed_system = true;
		}
	}

	// FIXME: Verify that this doesn't go out of bounds.
	sync_before_read((void*)data, length);
	memcpy(&ctx->base[ctx->BlockOff], data, length);
	sync_after_write(&ctx->base[ctx->BlockOff], length);
}

/**
 * Read data from the card using the current block offset.
 * @param slot Slot number.
 * @param data Buffer for the read data.
 * @param length Length of data to read, in bytes.
 */
void GCNCard_Read(int slot, void *data, u32 length)
{
	if (!GCNCard_IsEnabled(slot))
		return;
	GCNCard_ctx *const ctx = &memCard[slot];

	// FIXME: Verify that this doesn't go out of bounds.
	sync_before_read(&ctx->base[ctx->BlockOff], length);
	memcpy(data, &ctx->base[ctx->BlockOff], length);
	sync_after_write(data, length);
}

/**
 * Get the card's "code" value.
 * @param slot Slot number.
 * @param Card's "code" value.
 */
u32 GCNCard_GetCode(int slot)
{
	if (!GCNCard_IsEnabled(slot))
		return 0;
	return memCard[slot].code;
}

/**
 * Set the current block offset. (ERASE mode; uses sector values only.)
 * @param slot Slot number.
 * @param Data Block offset (sector values) from the EXI command.
 */
void GCNCard_SetBlockOffset_Erase(int slot, u32 data)
{
	if (!GCNCard_IsEnabled(slot))
		return;

	u32 BlockOff = (((u32)data>>16)&0xFF)  << 17;
	BlockOff    |= (((u32)data>> 8)&0xFF)  << 9;
	memCard[slot].BlockOff = BlockOff;
}
