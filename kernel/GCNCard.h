// Nintendont (kernel): GameCube Memory Card functions.
// Used by EXI.c.

#ifndef __GCNCARD_H__
#define __GCNCARD_H__

#include "global.h"

/**
 * Is a memory card enabled?
 * @param slot Slot number. (0 == Slot A, 1 == Slot B)
 * @return 0 if disabled; 1 if enabled.
 */
u32 GCNCard_IsEnabled(int slot);

/**
 * Load a memory card from disk.
 * @param slot Slot number. (0 == Slot A, 1 == Slot B)
 * NOTE: Slot B is not valid on Triforce.
 * @return 0 on success; non-zero on error.
 */
int GCNCard_Load(int slot);

/**
 * Get the total size of the loaded memory cards.
 * @return Total size, in bytes.
 */
u32 GCNCard_GetTotalSize(void);

/**
* Check if the memory cards have changed.
* @return True if either memory card has changed; false if not.
*/
bool GCNCard_CheckChanges(void);

/**
 * Save the memory card(s).
 */
void GCNCard_Save(void);

/** Functions used by EXIDeviceMemoryCard(). **/

void GCNCard_ClearWriteCount(int slot);

/**
 * Set the current block offset.
 * @param slot Slot number.
 * @param data Block offset from the EXI command.
 */
void GCNCard_SetBlockOffset(int slot, u32 data);

/**
 * Write data to the card using the current block offset.
 * @param slot Slot number.
 * @param data Data to write.
 * @param length Length of data to write, in bytes.
 */
void GCNCard_Write(int slot, const void *data, u32 length);

/**
 * Read data from the card using the current block offset.
 * @param slot Slot number.
 * @param data Buffer for the read data.
 * @param length Length of data to read, in bytes.
 */
void GCNCard_Read(int slot, void *data, u32 length);

/**
 * Get the card's "code" value.
 * @param slot Slot number.
 * @param Card's "code" value.
 */
u32 GCNCard_GetCode(int slot);

/**
 * Set the current block offset. (ERASE mode; uses sector values only.)
 * @param slot Slot number.
 * @param Data Block offset (sector values) from the EXI command.
 */
void GCNCard_SetBlockOffset_Erase(int slot, u32 data);

#endif /* __GCNCARD_H__ */
