#include "JVSIO.h"
#include "JVSIOMessage.h"
#include "vsprintf.h"

#ifndef DEBUG_JVSIO
#define dbgprintf(...)
#else
extern int dbgprintf( const char *fmt, ...);
#endif

vu32 d10_0		= 0xFF;
vu32 d10_1		= 0xFE;
u8	*res		= NULL;
u8	resp		= 0;
u32	DataPos	= 0;

extern vu32 TRIGame;

void JVSIOCommand( char *DataIn, char *DataOut )
{
	static u32 coin	= 0;
	static u32 mcoin = 9;

	//dbgprintf("JVS-IO (%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X)\n", DataIn[DataPos],
	//																DataIn[DataPos+1],
	//																DataIn[DataPos+2],
	//																DataIn[DataPos+3],
	//																DataIn[DataPos+4],
	//																DataIn[DataPos+5],
	//																DataIn[DataPos+6],
	//																DataIn[DataPos+7] );

	switch(TRIGame)
	{
		case 1:
		{
			sync_before_read( (void*)0x00690AC0, 0x20 );
			write32( 0x00690AC0, 1 );	// MGP2 credits
			sync_after_write( (void*)0x00690AC0, 0x20 );
		} break;
		case 2:	// VS42006
		{
			if( read32( 0x0210C08 ) == 0x386000A8 )	// EXPORT
			{
				sync_before_read( (void*)0x064C6A0, 0x20 );
				write32( 0x064C6B0, 1 );			// credits
				sync_after_write( (void*)0x064C6A0, 0x20 );
			}
			if( read32( 0x024E888 ) == 0x386000A8 ) // JAPAN
			{
				sync_before_read( (void*)0x0694BA0, 0x20 );
				write32( 0x0694BB0, 1 );			// credits
				sync_after_write( (void*)0x0694BA0, 0x20 );
			}
		} break;
		case 3:
		{
			sync_before_read( (void*)0x00400DE0, 0x20 );
			write32( 0x00400DE8, 1 );			// FZeroAX credits
			sync_after_write( (void*)0x00400DE0, 0x20 );

			sync_before_read( (void*)0x003BC400, 0x20 );
			write32( 0x003BC400, 0x803BB940 );	// Crash hack
			sync_after_write( (void*)0x003BC400, 0x20 );

			sync_before_read( (void*)0x03CFBE0, 0x20 );
			u32 val = read32( 0x03CFBF4 ) & 0xFFFF00FF;
			write32( 0x03CFBF4, val | 0x0400 );
			sync_after_write( (void*)0x03CFBE0, 0x20 );
		} break;
		case 4:
		{
			sync_before_read( (void*)0x00577760, 0x20 );
			write32( 0x00577760, 1 );	// MGP credits
			sync_after_write( (void*)0x00577760,0x20 );
		} break;
	}

	JVSIOMessage();

	u32 i,j;
	unsigned char jvs_io_buffer[0x80];
	int nr_bytes = DataIn[DataPos + 3]; // byte after e0 xx
	int jvs_io_length = 0;

	for( i=0; i<nr_bytes + 3; ++i)
		jvs_io_buffer[jvs_io_length++] = DataIn[ DataPos + 1 + i ];

	int node = jvs_io_buffer[1];
	JVSIOstart(0);
	addDataByte(1);

	unsigned char *jvs_io = jvs_io_buffer + 3;
	jvs_io_length--; // checksum

	while( jvs_io < (jvs_io_buffer + jvs_io_length) )
	{
		int cmd = *jvs_io++;
	//	dbgprintf("JVS IO, node=%d, cmd=%02X\n", node, cmd );

		switch( cmd )
		{
			// read ID data
			case 0x10:
			{
				addDataByte(1);
				if( TRIGame == 2 )
					addDataString("SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551;Ver1.00");
				else
			addDataString("namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder");
			} break;
			// get command format revision
			case 0x11:
				addDataByte(1);
				addDataByte(0x11);
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:cmd revision\n");
#endif
			break;
			// get JVS revision
			case 0x12:
				addDataByte(1);
				addDataByte(0x20);
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:jvs revision\n");
#endif
			break;
			// get supported communications versions
			case 0x13:
				addDataByte(1);
				addDataByte(0x10);
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:com revision\n");
#endif
			break;
			// get slave features
			/*
				0x01: Player count, Bit per channel
				0x02: Coin slots
				0x03: Analog-in
				0x04: Rotary
				0x05: Keycode
				0x06: Screen, x, y, ch
				....: unused
				0x10: Card
				0x11: Hopper-out
				0x12: Driver-out
				0x13: Analog-out
				0x14: Character, Line (?)
				0x15: Backup
			*/
			case 0x14:
				addDataByte(1);
				switch(TRIGame)
				{
					case 2:
					{
						// 2 Player, 2 Coin slots, 4 Analogs, 8Bit out
						addDataBuffer((void *)"\x01\x02\x0D\x00", 4);
						addDataBuffer((void *)"\x02\x02\x00\x00", 4);
						addDataBuffer((void *)"\x03\x04\x00\x00", 4);
						addDataBuffer((void *)"\x10\x01\x00\x00", 4);
						addDataBuffer((void *)"\x12\x08\x00\x00", 4);
						addDataBuffer((void *)"\x00\x00\x00\x00", 4);
					} break;
					case 3:
					{
						// 1 Player (12-bits), 1 Coin slot, 6 Analog-in
						addDataBuffer((void *)"\x01\x01\x0c\x00", 4);
						addDataBuffer((void *)"\x02\x01\x00\x00", 4);
						addDataBuffer((void *)"\x03\x06\x00\x00", 4);
						addDataBuffer((void *)"\x00\x00\x00\x00", 4);
					} break;
					default:
					{
						// 1 Player, 1 Coin slot, 3 Analogs, 8Bit out
						addDataBuffer((void *)"\x01\x01\x13\x00", 4);
						addDataBuffer((void *)"\x02\x01\x00\x00", 4);
						addDataBuffer((void *)"\x03\x03\x00\x00", 4);
						addDataBuffer((void *)"\x10\x01\x00\x00", 4);
						addDataBuffer((void *)"\x12\x08\x00\x00", 4);
						addDataBuffer((void *)"\x13\x08\x00\x00", 4);
						addDataBuffer((void *)"\x00\x00\x00\x00", 4);
					} break;
				}
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:GetFeatures\n");
#endif
			break;
			// convey ID of main board 
			case 0x15:
				while (*jvs_io++) {};
				addDataByte(1);
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:SentConfigString\n");
#endif
			break;
			// read switch inputs
			case 0x20:
			{
				//dbgprintf("JVS-IO:Read Switch Input\n");

				u32 PlayerCount			= *jvs_io++;
				u32 PlayerByteCount	= *jvs_io++;

				addDataByte(1);

				//u32 buttons = *(vu32*)0x0d806404;
				//u32 sticks  = *(vu32*)0x0d806408;

				// Test button
				//if( (buttons >> 16) & PAD_BUTTON_X )
				//	addDataByte(0x80);
				//else
				addDataByte(0x00);

				for( i=0; i < PlayerCount; ++i )
				{
					unsigned char PlayerData[3] = {0,0,0};

					// Service button
					//if( (buttons >> 16) & PAD_BUTTON_Y )
					//	PlayerData[0] |= 0x40;

					for( j=0; j < PlayerByteCount; ++j )
						addDataByte( PlayerData[j] );
				}
			} break;
			// read coin inputs
			case 0x21:
			{
				int slots = *jvs_io++;

			//	dbgprintf("JVS-IO:Get Coins Slots:%u\n", slots );

				if( mcoin )
					coin = !coin;

				addDataByte(1);
				while (slots--)
				{
					addDataByte(0);
					addDataByte(coin);
				}
			} break;
			// read analog inputs
			case 0x22:
			{
				addDataByte(1);	// status
				int players = *jvs_io++;

				for( i=0; i < players; ++i )
				{
					int val = 0;

						if (i < 4)
							val = 0x7FFF;
						else if (i < 6)
							val = 42 * 0x101;
						else
							val = 0;

					unsigned char player_data[2] = {val >> 8, val};

					addDataByte( player_data[0] );
					addDataByte( player_data[1] );
				}

			} break;
			case 0x23:	// rotary
			{
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:Rotary Input\n");
#endif
			} break;
			case 0x30:	// sub coins
			{
				u8 a = *jvs_io++;
#ifndef DEBUG_JVSIO
				jvs_io++;
				jvs_io++;
#else
				u8 b = *jvs_io++;
				u8 c = *jvs_io++;
				dbgprintf("%u,%u,%u\n", a, b, c );
#endif

				if( a == 1 )
					mcoin = 0;

				addDataByte(1);

			} break;
			case 0x32:	// general out
			{
#ifndef DEBUG_JVSIO
				jvs_io++;
				jvs_io++;
#else
				u8 a = *jvs_io++;
				u8 b = *jvs_io++;
				dbgprintf("JVS-IO:Gernal Output (%02X,%02X)\n", a, b );
#endif

				addDataByte(1);
			} break;
			case 0x70:	// custom namco's command subset
			{
#ifdef DEBUG_JVSIO
				dbgprintf("JVS-IO:Unknown (0x70) (%02X,%02X,%02X,%02X,%02X)\n", jvs_io[0], jvs_io[1], jvs_io[2], jvs_io[3], jvs_io[4] );
#endif
				jvs_io+=5;
			} break;
			case 0xF0:
			{
				if( *jvs_io++ == 0xD9 )
				{
#ifdef DEBUG_JVSIO
					dbgprintf("JVS-IO:JVS RESET\n");
#endif
				} else {
#ifdef DEBUG_JVSIO
					dbgprintf("JVS-IO:JVS RESET unknown reset value:%02X\n", DataIn[DataPos-1] );
#endif
				}
				addDataByte(1);

				d10_1 |= 1;
			} break;
			case 0xF1:
			{
				node = *jvs_io++;
#ifdef DEBUG_JVSIO
				dbgprintf( "JVS SET ADDRESS, node=%d\n", node);
#endif
				addDataByte(node == 1);
				d10_1 &= ~1;
			} break;
			case 0xF2:
#ifdef DEBUG_JVSIO
				dbgprintf("Rev mode Set: %x\nIgnored\n", *jvs_io );
#endif
			break;
			default:
			{
				dbgprintf("JVS:Unhandled CMD:%02X DataPos:%02X\n", cmd, DataPos-1 );
				hexdump( DataIn, 0x80 );
				Shutdown();
			} break;
		}
	}

	end();

	res[resp++] = DataIn[DataPos-1];

	extern int m_ptr;
	extern unsigned char m_msg[0x80];

	unsigned char *buf = m_msg;
	int len = m_ptr;
	res[resp++] = len;
	for (i=0; i<len; ++i)
		res[resp++] = buf[i];

	DataPos += nr_bytes + 4;
}
