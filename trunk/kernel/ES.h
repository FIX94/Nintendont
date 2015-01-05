#ifndef __ES_H__
#define __ES_H__

#include "global.h"
#include "string.h"
#include "Config.h"
#include "ff.h"
#include "NAND.h"

#define TICKET_SIZE		0x2A4

enum ESStatus
{
	ES_SUCCESS	= 0,
	ES_NFOUND	= -106,
	ES_FATAL	= -1017,
	ES_EHASH	= -1022,
	ES_ETIKTMD	= -1029,
	ES_EFAIL	= -4100,
};


#define SHA_INIT 0
#define SHA_UPDATE 1
#define SHA_FINISH 2

enum ESModuleHandles
{
	ES_FD = 0,
	SD_FD = 155,
};

enum ContentType
{
	CONTENT_REQUIRED=	(1<< 0),	// not sure
	CONTENT_SHARED	=	(1<<15),
	CONTENT_OPTIONAL=	(1<<14),
};

typedef struct
{
	u32 ID;				//	0	(0x1E4)
	u16 Index;			//	4	(0x1E8)
	u16 Type;			//	6	(0x1EA)
	u64 Size;			//	8	(0x1EC)
	u8	SHA1[20];		//  12	(0x1F4)
} __attribute__((packed)) Content;

typedef struct
{
	u32 SignatureType;		// 0x000
	u8	Signature[0x100];	// 0x004

	u8	Padding0[0x3C];		// 0x104
	u8	Issuer[0x40];		// 0x140

	u8	Version;			// 0x180
	u8	CACRLVersion;		// 0x181
	u8	SignerCRLVersion;	// 0x182
	u8	Padding1;			// 0x183

	u64	SystemVersion;		// 0x184
	u64	TitleID;			// 0x18C 
	u32	TitleType;			// 0x194 
	u16	GroupID;			// 0x198 
	u8	Reserved[62];		// 0x19A 
	u32	AccessRights;		// 0x1D8
	u16	TitleVersion;		// 0x1DC 
	u16	ContentCount;		// 0x1DE 
	u16 BootIndex;			// 0x1E0
	u8	Padding3[2];		// 0x1E2 

	Content Contents[];		// 0x1E4 

} __attribute__((packed)) TitleMetaData;

typedef struct
{
	u32	SignatureType;			// 0x000
	u8	Signature[0x100];		// 0x004

	u8	Padding[0x3C];			// 0x104
	s8	SignatureIssuer[0x40];	// 0x140
	u8	DownloadContent[0x3F];	// 0x180
	u8	EncryptedTitleKey[0x10];// 0x1BF
	u8	Unknown;				// 0x1CF
	u64	TicketID;				// 0x1D0
	u32	ConsoleID;				// 0x1D8
	u64	TitleID;				// 0x1DC
	u16 UnknownA;				// 0x1E4
	u16 BoughtContents;			// 0x1E6
	u8	UknownB[0x08];			// 0x1E9
	u8	CommonKeyIndex;			// 0x1F1
	u8	UnknownC[0x30];			// 0x1F2
	u8	UnknownD[0x20];			// 0x222
	u16 PaddingA;				// 0x242
	u32 TimeLimitEnabled;		// 0x248
	u32 TimeLimit;				// 0x24C

} __attribute__((packed)) Ticket;


typedef struct
{
	u8	Unknown;				// 0x000
	u8	Padding[3];				// 0x001
	u64	TicketID;				// 0x004
	u32	ConsoleID;				// 0x00C
	u64	TitleID;				// 0x010
	u16 UnknownA;				// 0x
	u16 BoughtContents;			// 0x
	u8	UknownB[0x08];			// 0x
	u8	CommonKeyIndex;			// 0x
	u8	UnknownC[0x30];			// 0x
	u8	UnknownD[0x20];			// 0x
	u16 PaddingA;				// 0x
	u32 TimeLimitEnabled;		// 0x
	u32 TimeLimit;				// 0x

} __attribute__((packed)) TicketView;

typedef struct
{
	u64	TitleID;				//
	u16 Padding;				//
	u16 GroupID;				//

} __attribute__((packed)) UIDSYS;

u32 ES_Init( u8 *MessageHeap );

s32 LaunchTitle( u64 TitleID );
s32 ES_BootSystem( void );
s32 LoadModules( u32 IOSVersion );

s32 GetSharedContentID( void *ContentHash );
s32 GetUID( u64 *TitleID, u16 *UID );
u64 GetTitleID( void );
void iCleanUpTikTMD( void );

void GetTicketView( u8 *Ticket, u8 *oTicketView );
s32 ESP_OpenContent( u64 TitleID, u32 ContentID );
s32 ESP_LaunchTitle( u64 *TitleID, u8 *TikView );

#endif
