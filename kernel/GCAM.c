#include "GCAM.h"
#include "JVSIO.h"
#include "Config.h"
#include "DI.h"
#ifndef DEBUG_GCAM
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

char *Bufo = NULL;
char *Bufi = NULL;
char *Buf	 = NULL;
u32 STRInit = 0;
u32 SystemRegion;

extern vu32 d10_0;
extern vu32 d10_1;
extern u8 resp;
extern u8 *res;
extern u32 DataPos;
extern u32 TRIGame;

u8	*CARDMemory;
u8	*CARDReadPacket;
u8	*CARDBuffer;

u32 CARDMemorySize;
u32 CARDIsInserted;
u32 CARDCommand;
u32 CARDClean;
u32 CARDWriteLength;
u32 CARDWrote;
u32 CARDReadLength;
u32 CARDRead;
u32 CARDBit;
u32 CARDStateCallCount;
u32 CARDOffset;
u32 FirstCMD;

static u32 BufsAllocated = 0;
void GCAMInit( void )
{
	if(BufsAllocated == 0)
	{
		Bufo= (char*)malloca( 0x80, 32 );
		Bufi= (char*)malloca( 0x80, 32 );
		Buf = (char*)malloca( 0x80, 32 );
		res = (u8*)  malloca( 0x80, 32 );

		CARDMemory = (u8*)malloca( 0xD0, 32 );
		CARDReadPacket = (u8*)malloca( 0xDB, 32 );
		CARDBuffer = (u8*)malloca( 0x100, 32 );
		BufsAllocated = 1;
	}
	memset32( Bufo, 0, 0x80 );
	memset32( Bufi, 0, 0x80 );
	memset32( Buf,  0, 0x80 );
	memset32( res,  0, 0x80 );

	memset32( CARDMemory, 0, 0xD0 );
	memset32( CARDReadPacket, 0, 0xDB );
	memset32( CARDBuffer, 0, 0x100 );

	memset32( (void*)GCAM_BASE, 0xdeadbeef, 0x40 );
	sync_after_write( (void*)GCAM_BASE, 0x40 );

	CARDMemorySize = 0;
	CARDIsInserted = 0;
	CARDCommand = 0;
	CARDClean = 0;
	CARDWriteLength = 0;
	CARDWrote = 0;
	CARDReadLength = 0;
	CARDRead = 0;
	CARDBit = 0;
	CARDStateCallCount = 0;
	CARDOffset = 0;
	FirstCMD = 0;
}

static const char *CARD_NAME_GP1 = "/saves/GP1.bin";
static const char *CARD_NAME_GP2 = "/saves/GP2.bin";
static const char *CARD_NAME_AX = "/saves/AX.bin";
static const char *CARD_NAME_VS4 = "/saves/VS4.bin";
static const char *CARD_NAME_DEF = "csave.bin";

const char *GCAMGetCARDName()
{
	const char *name;
	switch(TRIGame)
	{
		case TRI_GP1:
			name = CARD_NAME_GP1;
			break;
		case TRI_GP2:
			name = CARD_NAME_GP2;
			break;
		case TRI_AX:
			name = CARD_NAME_AX;
			break;
		case TRI_VS4:
			name = CARD_NAME_VS4;
			break;
		default:
			name = CARD_NAME_DEF;
			break;
	}
	return name;
}

u8 GCAMCARDIsValid( u8 *mem )
{
	u8 valid = 0;
	switch(TRIGame)
	{
		case TRI_GP1:
		case TRI_GP2:
			valid = (memcmp(mem, "MKA", 3) == 0);
			break;
		case TRI_AX:
			valid = (memcmp(mem+0x8A, "SEGABGG", 7) == 0);
			break;
		default:
			break;
	}
	return valid;
}
			
void GCAMCARDCleanStatus( u32 status )
{
	if(status < 0x96)
		CARDClean = 2;
	else
		CARDClean = 0;
#ifdef DEBUG_CARD
	dbgprintf("CARDClean=%d\r\n", CARDClean);
#endif
}

void GCAMCARDCommand( char *DataIn, char *DataOut )
{
	if( DataIn[DataPos] == 1 && DataIn[DataPos+1] == 0x05 )
	{
	//	dbgprintf("CARDGetReply(%02X)\n", CARDCommand );

		if( CARDReadLength )
		{
			res[resp++] = 0x32;

			u32 ReadLength = CARDReadLength - CARDRead;
			if( TRIGame == TRI_AX )
			{
				if( ReadLength > 0x2F )
					ReadLength = 0x2F;
			}
			res[resp++] = ReadLength;	// 0x2F (max size per packet)

			memcpy( res+resp, CARDReadPacket+CARDRead, ReadLength );

			resp			+= ReadLength;
			CARDRead	+= ReadLength;
							
		//	dbgprintf("CARDRead: %u/%u\n", CARDRead, CARDReadLength );									

			if( CARDRead >= CARDReadLength )
				CARDReadLength = 0;

		//	hexdump( res, 0x80 );

			DataPos += DataIn[DataPos] + 1;

			return;
		}

		res[resp++] = 0x32;
		u32 CMDLenO	= resp;
		res[resp++] = 0x00;	// len
							
		res[resp++] = 0x02; //
		u32 ChkStart = resp;

		res[resp++] = 0x00;	// 0x00		 len
		switch(CARDCommand)
		{
			case CARD_INIT:
			{
				res[resp++] = 0x10;	// 0x01
				res[resp++] = 0x00;	// 0x02
				res[resp++] = 0x30;	// 0x03
			} break;
			case CARD_IS_PRESENT:
			{
				res[resp++] = 0x40;	// 0x01
				res[resp++] = 0x22;	// 0x02
				res[resp++] = 0x30;	// 0x03
			} break;
			case CARD_GET_CARD_STATE:
			{
				res[resp++] = 0x20;	// 0x01
				res[resp++] = 0x20|CARDBit;	// 0x02
				/*
					bit 0: PLease take your card
					bit 1: endless waiting casues UNK_E to be called
				*/
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_7A:
			{
				res[resp++] = 0x7A;	// 0x01
				res[resp++] = 0x00;	// 0x02
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_78:
			{
				res[resp++] = 0x78;	// 0x01
				res[resp++] = 0x00;	// 0x02
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_WRITE_INFO:
			{
				res[resp++] = 0x7C;	// 0x01
				res[resp++] = 0x02;	// 0x02
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_D0:
			{
				res[resp++] = 0xD0;	// 0x01
				res[resp++] = 0x00;	// 0x02
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_80:
			{
				res[resp++] = 0x80;	// 0x01

				if( TRIGame == TRI_AX )
					res[resp++] = 0x01;	// 0x02
				else
					res[resp++] = 0x31;	// 0x02

				res[resp++] = 0x30;	// 0x03
			} break;
			case CARD_CLEAN_CARD:
			{
				res[resp++] = 0xA0;	// 0x01
				res[resp++] = 0x02;	// 0x02
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_LOAD_CARD:
			{
				res[resp++] = 0xB0;	// 0x01
				res[resp++] = 0x02;	// 0x02
				res[resp++] = 0x30;	// 0x03
			} break;
			case CARD_WRITE:
			{
				res[resp++] = 0x53;	// 0x01
				res[resp++] = 0x02;	// 0x02
				res[resp++] = 0x00;	// 0x03
			} break;
			case CARD_READ:
			{
				res[resp++] = 0x33;	// 0x01
				res[resp++] = 0x02;	// 0x02
				res[resp++] = 0x53;	// 0x03
			} break;									
		}

		res[resp++] = 0x30;	// 0x04
		res[resp++] = 0x00;	// 0x05

		res[resp++] = 0x03;	// 0x06

		res[ChkStart] = resp-ChkStart;	// 0x00 len

		u32 i;
		res[resp] = 0;		// 0x07
		for( i=0; i < res[ChkStart]; ++i )
			res[resp] ^= res[ChkStart+i];
							
		resp++;	

		res[CMDLenO] = res[ChkStart] + 2;

	}
	else
	{

		memcpy( CARDBuffer + CARDOffset, DataIn+DataPos+1, DataIn[DataPos] );
		CARDOffset += DataIn[DataPos];

		//Check if we got complete CMD

		if( CARDBuffer[0] == 0x02 )
		{
			if( CARDBuffer[1] == CARDOffset - 2 )
			{
				if( CARDBuffer[CARDOffset-2] == 0x03 )
				{
					u32 cmd = CARDBuffer[2] << 24;
						cmd|= CARDBuffer[3] << 16;
						cmd|= CARDBuffer[4] <<  8;
						cmd|= CARDBuffer[5] <<  0;

					switch(cmd)
					{
						default:
						{
						//	dbgprintf("CARD:Unhandled cmd!\n");
						//	dbgprintf("CARD:[%08X]\n", cmd );
						//	hexdump( CARDBuffer, CARDOffset );
						} break;
						case 0x10000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDInit()\n");
		#endif
							CARDCommand = CARD_INIT;

							CARDWriteLength		= 0;
							CARDBit				= 0;
							CARDMemorySize			= 0;
							CARDStateCallCount	= 0;

						} break;
						case 0x20000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDGetState(%02X)\n", CARDBit );
		#endif
							CARDCommand = CARD_GET_CARD_STATE;

							if( TRIGame == TRI_AX && CARDMemorySize )
							{
								CARDStateCallCount++;
								if( CARDStateCallCount > 10 )
								{
									if( CARDBit & 2 )
										CARDBit &= ~2;
									else
										CARDBit |= 2;

									CARDStateCallCount = 0;
								}
							}

							if( CARDClean == 1 )
							{
								CARDClean = 2;
							} else if( CARDClean == 2 )
							{
								DIFinishAsync(); //DONT ever try todo file i/o async
								FIL fi;
								if( f_open_char( &fi, GCAMGetCARDName(), FA_READ|FA_OPEN_EXISTING ) == FR_OK )
								{
									if( fi.obj.objsize > 0 && fi.obj.objsize <= 0xD0 )
									{
										u8 *cmem = (u8*)malloca( fi.obj.objsize, 32 );
										u32 read;
										f_read( &fi, cmem, fi.obj.objsize, &read );
										if(GCAMCARDIsValid(cmem))
										{
											CARDMemorySize = fi.obj.objsize;
											if( TRIGame == TRI_AX )
												CARDBit = 2;
											else
												CARDBit = 1;
										}
										free(cmem);
									}
									f_close(&fi);
								}
								CARDClean = 0;
							}
						} break;
						case 0x40000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDIsPresent()\n");
		#endif
							CARDCommand = CARD_IS_PRESENT;
						} break;
						case 0x7A000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDUnknown7A()\n");
		#endif
							CARDCommand = CARD_7A;
							//hexdump( CARDBuffer+2, CARDOffset-2 );
						} break;
						case 0xB0000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDLoadCard()\n");
		#endif
							CARDCommand = CARD_LOAD_CARD;
						} break;
						case 0xA0000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDIsCleanCard?()\n");
		#endif
							CARDCommand = CARD_CLEAN_CARD;
							CARDClean = 1;
						} break;
						case 0x33000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDRead()\n");
		#endif
							CARDCommand		= CARD_READ;

							//Prepare read packet
							memset( CARDReadPacket, 0, 0xDB );
							u32 POff=0;

							DIFinishAsync(); //DONT ever try todo file i/o async
							FIL cf;
							if( f_open_char( &cf, GCAMGetCARDName(), FA_READ|FA_OPEN_EXISTING ) == FR_OK )
							{
								if( cf.obj.objsize > 0 && cf.obj.objsize <= 0xD0 )
								{
									u8 *cmem = (u8*)malloca( cf.obj.objsize, 32 );
									u32 read;
									f_read( &cf, cmem, cf.obj.objsize, &read );
									if(GCAMCARDIsValid(cmem))
									{
										if( CARDMemorySize == 0 )
											CARDMemorySize = cf.obj.objsize;
										memcpy(CARDMemory, cmem, CARDMemorySize);
										sync_after_write(CARDMemory, CARDMemorySize);
										CARDIsInserted = 1;
									}
									free(cmem);
								}
								f_close( &cf );
							}

							CARDReadPacket[POff++] = 0x02;	// SUB CMD
							CARDReadPacket[POff++] = 0x00;	// SUB CMDLen

							CARDReadPacket[POff++] = 0x33;	// CARD CMD

							if( CARDIsInserted )
							{
								CARDReadPacket[POff++] = '1';	// CARD Status
							} else {
								CARDReadPacket[POff++] = '0';	// CARD Status
							}

							CARDReadPacket[POff++] = '0';		// 
							CARDReadPacket[POff++] = '0';		// 

							//Data reply
							sync_before_read(CARDMemory, CARDMemorySize);
							memcpy( CARDReadPacket + POff, CARDMemory, CARDMemorySize );
							POff += CARDMemorySize;

							CARDReadPacket[POff++] = 0x03;

							CARDReadPacket[1] = POff-1;	// SUB CMDLen

							u32 i;
							for( i=0; i < POff-1; ++i )
								CARDReadPacket[POff] ^= CARDReadPacket[1+i];

							POff++;

						//	hexdump( CARDReadPacket, POff );

							CARDReadLength	= POff;
							CARDRead	= 0;
						} break;
						case 0x53000000:
						{
							CARDCommand		= CARD_WRITE;

							CARDMemorySize		= CARDBuffer[1] - 9;

							memcpy( CARDMemory, CARDBuffer+9, CARDMemorySize );
		#ifdef DEBUG_CARD
							dbgprintf("CARDWrite: %u\n", CARDMemorySize );
		#endif
							if(GCAMCARDIsValid(CARDMemory))
							{
								DIFinishAsync(); //DONT ever try todo file i/o async
								FIL cf;
								if( f_open_char( &cf, GCAMGetCARDName(), FA_WRITE|FA_CREATE_ALWAYS ) == FR_OK )
								{
									u32 wrote;
									f_write( &cf, CARDMemory, CARDMemorySize, &wrote );
									f_close( &cf );
								}
							}
							CARDBit = 2;
							sync_after_write(CARDMemory, CARDMemorySize);
							CARDStateCallCount = 0;
						} break;
						case 0x78000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDUnknown78()\n");
		#endif
							CARDCommand	= CARD_78;
						} break;
						case 0x7C000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDWriteCardInfo()\n");
		#endif
							CARDCommand	= CARD_WRITE_INFO;
						//	hexdump( CARDBuffer, CARDOffset );
						} break;
						case 0x7D000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDPrint?()\n");
		#endif
							CARDCommand	= CARD_7D;
						} break;
						case 0x80000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDUnknown80()\n");
		#endif
							CARDCommand	= CARD_80;

							if( TRIGame != TRI_AX )
								CARDBit = 0;
						} break;
						case 0xD0000000:
						{
		#ifdef DEBUG_CARD
							dbgprintf("CARDUnknownD0()\n");
		#endif
							CARDCommand	= CARD_D0;

							if( TRIGame != TRI_AX )
								CARDBit = 0;
						} break;
					}

				//	hexdump( CARDBuffer, CARDOffset );

					CARDOffset = 0;
				}
			}
		}

		res[resp++] = 0x32;
		res[resp++] = 0x01;	// len
		res[resp++] = 0x06;	// OK

	}
}
void GCAMCommand( char *DataIn, char *DataOut )
{
	u32 i = 0;

	DataPos = 0;
	resp		= 0;

	u32 CMDLen = DataIn[++DataPos];
	DataPos++;

//	dbgprintf("GC-AM:Length:%u\n", CMDLen );
	
	memset32( res, 0, 0x80 );
	res[resp++] = 1;
	res[resp++] = 1;

	while( DataPos <= CMDLen )
	{
	//	dbgprintf("%02X,", DataIn[DataPos] );

		switch( DataIn[DataPos++] )
		{
			/* No reply */
			case 0x05:
				dbgprintf( "GC-AM: CMD 05 (%02X,%02X,%02X,%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1], DataIn[DataPos+2], DataIn[DataPos+3], DataIn[DataPos+4] );
				DataPos += 5;
			break;
			/* No reply */
			case 0x08:
				dbgprintf( "GC-AM: CMD 08 (%02X,%02X,%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1], DataIn[DataPos+2], DataIn[DataPos+3] );
				DataPos += 4;
			break;
			/* 2 bytes out */
			case 0x10:
			{
				res[resp++] = 0x10;
				res[resp++] = 0x02;
		
				d10_0 = 0xFF;

				//u32 buttons = read32(TRIButtons);//*(vu32*)0x0d806404;
				//u32 sticks  = *(vu32*)0x0d806408;

				// Service button
				//if( buttons & PAD_BUTTON_X )
				//	d10_0 &= ~0x80;

				// Test button
				//if( buttons & PAD_TRIGGER_Z )
				//	d10_0 &= ~0x40;

				//Switch Status
				res[resp++] = d10_0;
				//Reset Status
				res[resp++] = d10_1;

				//dbgprintf("ReadStatus and Switches(%02X %02X,%02X)\n", DataIn[DataPos], d10_0, d10_1 );
				
				DataPos += DataIn[DataPos] + 1;
			} break;
			case 0x11:
			{
				dbgprintf("GetSerial(%02X)\n", DataIn[DataPos] );

				res[resp++] = 0x11;
				res[resp++] = 0x10;

				memcpy( res + resp, "AADE01A28834511", 16 );

				resp += 0x10;

				DataPos += DataIn[DataPos] + 1;
			} break;
			/* No reply */
			case 0x12:
			{
				dbgprintf("Unknown(12) (%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1] );

				DataPos += DataIn[DataPos] + 1;
			} break;
			/* 4 half-words out */
		//	case 0x14:
			/* 2 bytes out */
			case 0x15:
			{
				dbgprintf("GetFirmware Version(%02X)\n", DataIn[DataPos++] );

				res[resp++] = 0x15;
				res[resp++] = 0x02;

				// FIRM VERSION
				res[resp++] = 0x00;
				res[resp++] = 0x29; 

			} break;
			/* 2 bytes out */
			case 0x16:
			{
				dbgprintf("GetFPGA Version(%02X)\n", DataIn[DataPos++] );

				res[resp++] = 0x16;
				res[resp++] = 0x02;

				// FPGAVERSION
				res[resp++] = 0x07;
				res[resp++] = 0x06; 

			} break;
			case 0x1F:
			{
				dbgprintf("GetRegion(%02X,%02X,%02X,%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1], DataIn[DataPos+2], DataIn[DataPos+3], DataIn[DataPos+4] );

				unsigned char string[0x14];

				switch(SystemRegion)
				{
					case REGION_USA:
						memcpy(string,
							"\x00\x00\x30\x00"
							"\x02\xfd\x00\x00" // USA
							"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff",
								0x14);
						break;
					case REGION_EXPORT:
						memcpy(string,
							"\x00\x00\x30\x00"
							"\x03\xfc\x00\x00" // export
							"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff",
								0x14);
						break;
					default:
						memcpy(string,
							"\x00\x00\x30\x00"
							"\x01\xfe\x00\x00" // JAPAN
							"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff",
								0x14);
						break;
				}
				res[resp++] = 0x1F;
				res[resp++] = 0x14;

				for (i=0; i<0x14; ++i)
					res[resp++] = string[i];

				DataPos += 5;
			} break;
			/* No reply */
			case 0x21:
				dbgprintf( "GC-AM: CMD 21 (%02X,%02X,%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1], DataIn[DataPos+2], DataIn[DataPos+3] );

				DataPos += 4;
			break;
			/* No reply */
			case 0x22:
				dbgprintf( "GC-AM: CMD 22 (%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1], DataIn[DataPos+2], DataIn[DataPos+3],
																						DataIn[DataPos+4], DataIn[DataPos+5], DataIn[DataPos+6], DataIn[DataPos+7] );
				DataPos += DataIn[DataPos] + 1;
			break;
			/* 3 byte out */
			//case 0x23:
			/* flexible length out */
			case 0x31:				
				if(DataIn[DataPos])
				{
					// All commands are OR'd with 0x80
					// Last byte (ptr(5)) is checksum which we don't care about
					u32 cmd = (DataIn[DataPos+1]^0x80) << 24;
							cmd|=  DataIn[DataPos+2] << 16;
							cmd|=  DataIn[DataPos+3] <<  8;
						
					//dbgprintf( "GC-AM: CMD 31 (SERIAL) (%08X)\n", cmd );
					
					// Serial - Wheel
					if( cmd == 0x7FFFFF00 )
					{
						res[resp++] = 0x31;
						res[resp++] = 0x03;
				
						res[resp++] = 'C';
						res[resp++] = '0';

						if( STRInit == 0 )
						{
							res[resp++] = '1';
							STRInit = 1;
						} else {
							res[resp++] = '6';
						}
					}
				}
				DataPos += DataIn[DataPos] + 1;
			break;
			// CARD-Interface
			case 0x32:
			{
				if( !DataIn[DataPos] )
				{
					res[resp++] = 0x32;
					res[resp++] = 0x00;	// len

				} else {

					dbgprintf("CARD(32) (%02X) (", DataIn[DataPos] );
					for( i=0; i < DataIn[DataPos]; ++i )
					{
						dbgprintf("%02X,", DataIn[DataPos+i+1] );
					}
					dbgprintf(")\n");

					GCAMCARDCommand( DataIn, DataOut );
				}

				DataPos += DataIn[DataPos] + 1;
			} break;
			case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:
			case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:
			{
				JVSIOCommand( DataIn, DataOut );
			} break;
			/* No reply */
			case 0x60:
			{
				dbgprintf("Unknown(60) (%02X,%02X,%02X)\n", DataIn[DataPos], DataIn[DataPos+1], DataIn[DataPos+2] );
				DataPos += DataIn[DataPos] + 1;
			} break;
			case 0x00:
			{
			} break;
			default:
			{
				dbgprintf("GC-AM:Unhandled CMD:0x%02X DataPos:0x%02X\n", DataIn[DataPos-1], DataPos-1 );
				hexdump( DataIn, 0x80 );
				Shutdown();
			} break;
		}
	}

	res[1] = resp - 2;

	u32 chk=0;
	for( i=0; i < 0x7F; i++)
		chk+=res[i];

	res[0x7F] = ~(chk & 0xFF);

	//Check checksum (???)
	//chk = 0;
	//for( i=0; i < 0x80; i++)
	//	chk+=res[i];

	memcpy( DataOut, res, 0x80 );
}
void GCAMUpdateRegisters( void )
{
	u32 i;

	u32 *GInterface	 = (u32*)(GCAM_BASE);
	u32 *GInterfaceS = (u32*)(GCAM_SHADOW);
	
	sync_before_read( (void*)GCAM_BASE, 0x40 );

	if( read32(GCAM_CONTROL) != 0xdeadbeef )
	{
		if( read32( GCAM_CONTROL ) & (~3) )
		{
			write32( GCAM_CONTROL, 0xdeadbeef );
			sync_after_write( (void*)GCAM_BASE, 0x40 );
			return;
		}

		/*write32( GCAM_SCONTROL, read32(GCAM_CONTROL) & 3 );
		clear32( GCAM_SSTATUS, 0x14 );

		write32( GCAM_CONTROL, 0xdeadbeef );
		write32( GCAM_RETURN, 0xdeadbeef );
		write32( GCAM_STATUS, 0xdeadbeef );

		sync_after_write( (void*)GCAM_BASE, 0x40 );*/
		
		for( i=0; i < 5; ++i )
		{
			if( GInterface[i] != 0xdeadbeef )
			{
				GInterfaceS[i] = GInterface[i];
				GInterface[i]  = 0xdeadbeef;
			}
		}

		switch( read32(GCAM_SCMD) >> 24 )
		{
			case 0x00:
			{
				//dbgprintf("CARD:Warning unknown command!\n");
			} break;
			case 0x50:
			{
				char	*datain		= (char*)P2C( read32(GCAM_SCMD_1) );
				u32		lenin			= read32(GCAM_SCMD_2);

				char	*dataout	= (char*)P2C( read32(GCAM_SCMD_3) );
				u32		lenout		= read32(GCAM_SCMD_4);

#ifdef DEBUG_GCAM
				dbgprintf("SI:Transfer( %p, %u, %p, %u )\n", datain, lenin, dataout, lenout );
				hexdump( datain, lenin );
#endif

				sync_before_read_align32(datain, lenin);
				switch( datain[0] )
				{
				//	dbgprintf("[%02X]", datain[0] );
					case 0x00:
					{
						W32( (u32)dataout, 0x10110800 );
				//		dbgprintf("Reset(0x%p)\n", dataout );
					} break;
					default:
					// CMD_DIRECT
					case 0x40:
					// CMD_ORIGIN
					case 0x41:
					// CMD_RECALIBRATE
					case 0x42:
					{
						memset( dataout, 0, lenout );
					} break;
				}

				sync_after_write_align32(dataout, lenout);

				//hexdump( dataout, lenout );

				//while( read32(GCAM_CONTROL) & 1 )
				//	clear32( GCAM_CONTROL, 1 );
				
				//while( (read32(GCAM_SSTATUS) & 0x10) != 0x10 )
				//	set32( GCAM_SSTATUS, 0x10 );

			} break;
			case 0x70:
			{
				char	*datain		= (char*)P2C( read32(GCAM_SCMD_1) );
				char	*dataout	= (char*)P2C( read32(GCAM_SCMD_2) );

				//dbgprintf("GC-AM:Command( %p, %u, %p, %u )\n", datain, 0x80, dataout, 0x80 );

				sync_before_read_align32(datain, 0x80);

				memcpy( Bufi, datain, 0x80 );

				GCAMCommand( Bufi, Buf );

				if( FirstCMD == 0 )
				{
					memset32( dataout, 0, 0x80 );
					FirstCMD = 1;
				} else {
					memcpy( dataout, Bufo, 0x80 );
					memcpy( Bufo, Buf, 0x80 );
				}

				sync_after_write_align32(dataout, 0x80);

				//hexdump( dataout, 0x10 );

				//while( read32(GCAM_CONTROL) & 1 )
				//	clear32( GCAM_CONTROL, 1 );
				
				//while( (read32(GCAM_SSTATUS) & 0x10) != 0x10 )
				//	set32( GCAM_SSTATUS, 0x10 );
			} break;
			default:
			{
				dbgprintf("Unhandled cmd:%02X\n", read32(GCAM_SCMD)  );
				Shutdown();
			} break;
		}
		//to be 100% sure we dont ever read a still cached block in ppc
		write32( GCAM_CONTROL, 0xdeadbeef );
		sync_after_write( (void*)GCAM_BASE, 0x40 );
	}
}
