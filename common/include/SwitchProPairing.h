#ifndef _SWITCH_PRO_PAIRING_H_
#define _SWITCH_PRO_PAIRING_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SWITCH_PRO_PAIRING_MAGIC    0x4e535042u
#define SWITCH_PRO_PAIRING_VERSION  3u
#define SWITCH_PRO_PAIRING_ARM_ADDR 0x132f3200u
#define SWITCH_PRO_PAIRING_PPC_ADDR 0x932f3200u
#define SWITCH_PRO_PAIRING_PATH     "wiiu/bloopair/nintendont-switch-pro.bin"
#define SWITCH_PRO_PAIRING_TYPE     0x24u
#define SWITCH_PRO_PAIRING_KEY_TYPE_UNKNOWN 0xffu
#define SWITCH_PRO_PAIRING_MAX_CONTROLLERS 4u

typedef struct __attribute__((packed)) {
	uint8_t controller_bda[6];
	uint8_t hci_link_key[16];
	uint8_t key_type;
	uint8_t controller_type;
	uint16_t vendor_id;
	uint16_t product_id;
	uint8_t reserved[2];
} SwitchProPairingEntry;

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t size;
	uint8_t console_bda[6];
	uint8_t count;
	uint8_t reserved;
	SwitchProPairingEntry controllers[SWITCH_PRO_PAIRING_MAX_CONTROLLERS];
	uint32_t checksum;
} SwitchProPairing;

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t size;
	uint8_t controller_bda[6];
	uint8_t console_bda[6];
	uint8_t hci_link_key[16];
	uint8_t key_type;
	uint8_t controller_type;
	uint16_t vendor_id;
	uint16_t product_id;
	uint8_t reserved[2];
	uint32_t checksum;
} SwitchProPairingLegacyV2;

static inline void SwitchProPairingAddressToLwbt(const uint8_t source[6],
	uint8_t destination[6])
{
	size_t i;
	for(i = 0; i < 6; i++)
		destination[i] = source[5 - i];
}

static inline void SwitchProPairingCopyHciLinkKey(
	const SwitchProPairingEntry *pairing, uint8_t destination[16])
{
	memcpy(destination, pairing->hci_link_key, 16);
}

static inline uint32_t SwitchProPairingChecksum(const SwitchProPairing *pairing)
{
	const uint8_t *bytes = (const uint8_t*)pairing;
	uint32_t hash = 2166136261u;
	size_t i;
	for(i = 0; i < offsetof(SwitchProPairing, checksum); i++)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

static inline int SwitchProPairingIsValid(const SwitchProPairing *pairing)
{
	uint8_t console_or = 0;
	size_t i, j;
	if(pairing == NULL || pairing->magic != SWITCH_PRO_PAIRING_MAGIC ||
		pairing->version != SWITCH_PRO_PAIRING_VERSION ||
		pairing->size != sizeof(*pairing) || pairing->count == 0 ||
		pairing->count > SWITCH_PRO_PAIRING_MAX_CONTROLLERS)
		return 0;
	for(i = 0; i < sizeof(pairing->console_bda); i++)
		console_or |= pairing->console_bda[i];
	if(!console_or)
		return 0;
	for(i = 0; i < pairing->count; i++)
	{
		const SwitchProPairingEntry *entry = &pairing->controllers[i];
		uint8_t address_or = 0, key_or = 0;
		if(entry->controller_type != SWITCH_PRO_PAIRING_TYPE ||
			entry->vendor_id != 0x057e || entry->product_id != 0x2009)
			return 0;
		for(j = 0; j < sizeof(entry->controller_bda); j++)
			address_or |= entry->controller_bda[j];
		for(j = 0; j < sizeof(entry->hci_link_key); j++)
			key_or |= entry->hci_link_key[j];
		if(!address_or || !key_or)
			return 0;
		for(j = 0; j < i; j++)
			if(memcmp(entry->controller_bda,
				pairing->controllers[j].controller_bda, 6) == 0)
				return 0;
	}
	return pairing->checksum == SwitchProPairingChecksum(pairing);
}

static inline int SwitchProPairingFilterSupported(
	const SwitchProPairing *source, SwitchProPairing *filtered)
{
	uint8_t console_or = 0;
	size_t i, j;
	if(source == NULL || filtered == NULL ||
		source->magic != SWITCH_PRO_PAIRING_MAGIC ||
		source->version != SWITCH_PRO_PAIRING_VERSION ||
		source->size != sizeof(*source) || source->count == 0 ||
		source->count > SWITCH_PRO_PAIRING_MAX_CONTROLLERS ||
		source->checksum != SwitchProPairingChecksum(source))
		return 0;
	for(i = 0; i < sizeof(source->console_bda); i++)
		console_or |= source->console_bda[i];
	if(!console_or)
		return 0;

	memset(filtered, 0, sizeof(*filtered));
	filtered->magic = SWITCH_PRO_PAIRING_MAGIC;
	filtered->version = SWITCH_PRO_PAIRING_VERSION;
	filtered->size = sizeof(*filtered);
	memcpy(filtered->console_bda, source->console_bda,
		sizeof(filtered->console_bda));
	for(i = 0; i < source->count; i++)
	{
		const SwitchProPairingEntry *entry = &source->controllers[i];
		uint8_t address_or = 0, key_or = 0;
		if(entry->controller_type != SWITCH_PRO_PAIRING_TYPE ||
			entry->vendor_id != 0x057e || entry->product_id != 0x2009)
			continue;
		for(j = 0; j < sizeof(entry->controller_bda); j++)
			address_or |= entry->controller_bda[j];
		for(j = 0; j < sizeof(entry->hci_link_key); j++)
			key_or |= entry->hci_link_key[j];
		if(!address_or || !key_or)
			return 0;
		for(j = 0; j < filtered->count; j++)
			if(memcmp(entry->controller_bda,
				filtered->controllers[j].controller_bda, 6) == 0)
				return 0;
		filtered->controllers[filtered->count++] = *entry;
	}
	if(filtered->count == 0)
		return 0;
	filtered->checksum = SwitchProPairingChecksum(filtered);
	return 1;
}

static inline uint32_t SwitchProPairingLegacyV2Checksum(
	const SwitchProPairingLegacyV2 *pairing)
{
	const uint8_t *bytes = (const uint8_t*)pairing;
	uint32_t hash = 2166136261u;
	size_t i;
	for(i = 0; i < offsetof(SwitchProPairingLegacyV2, checksum); i++)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

static inline int SwitchProPairingUpgradeLegacyV2(
	const SwitchProPairingLegacyV2 *old, SwitchProPairing *pairing)
{
	uint8_t address_or = 0, console_or = 0, key_or = 0;
	size_t i;
	if(old == NULL || pairing == NULL || old->magic != SWITCH_PRO_PAIRING_MAGIC ||
		old->version != 2 || old->size != sizeof(*old) ||
		old->controller_type != SWITCH_PRO_PAIRING_TYPE ||
		old->vendor_id != 0x057e || old->product_id != 0x2009 ||
		old->checksum != SwitchProPairingLegacyV2Checksum(old))
		return 0;
	for(i = 0; i < 6; i++) { address_or |= old->controller_bda[i]; console_or |= old->console_bda[i]; }
	for(i = 0; i < 16; i++) key_or |= old->hci_link_key[i];
	if(!address_or || !console_or || !key_or)
		return 0;
	memset(pairing, 0, sizeof(*pairing));
	pairing->magic = SWITCH_PRO_PAIRING_MAGIC;
	pairing->version = SWITCH_PRO_PAIRING_VERSION;
	pairing->size = sizeof(*pairing);
	memcpy(pairing->console_bda, old->console_bda, 6);
	pairing->count = 1;
	memcpy(pairing->controllers[0].controller_bda, old->controller_bda, 6);
	memcpy(pairing->controllers[0].hci_link_key, old->hci_link_key, 16);
	pairing->controllers[0].key_type = old->key_type;
	pairing->controllers[0].controller_type = old->controller_type;
	pairing->controllers[0].vendor_id = old->vendor_id;
	pairing->controllers[0].product_id = old->product_id;
	pairing->checksum = SwitchProPairingChecksum(pairing);
	return 1;
}

typedef char SwitchProPairingEntrySizeCheck[(sizeof(SwitchProPairingEntry) == 30) ? 1 : -1];
typedef char SwitchProPairingSizeCheck[(sizeof(SwitchProPairing) == 140) ? 1 : -1];
typedef char SwitchProPairingLegacySizeCheck[(sizeof(SwitchProPairingLegacyV2) == 48) ? 1 : -1];
typedef char SwitchProPairingAddressAliasCheck[
	((SWITCH_PRO_PAIRING_PPC_ADDR & 0x1fffffffu) == SWITCH_PRO_PAIRING_ARM_ADDR) ? 1 : -1];

#endif
