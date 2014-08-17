/*

  in_cube Gamecube Stream Player for Winamp
  by hcs

  includes work by Destop and bero

*/

// DLS (a.k.a. DTK, TRK, ADP)
// uses same algorithm as XA, apparently
// ADP decoder function by hcs, reversed from dtkmake (trkmake v1.4)

#define ONE_BLOCK_SIZE		32
#define SAMPLES_PER_BLOCK	28

short ADPDecodeSample( int bits, int q, long * hist1p, long * hist2p) {
	long hist=0,cur;
	long hist1=*hist1p,hist2=*hist2p;

	switch( q >> 4 )
	{
		case 0:
			hist = 0;
			break;
		case 1:
			hist = (hist1 * 0x3c);
			break;
		case 2:
			hist = (hist1 * 0x73) - (hist2 * 0x34);
			break;
		case 3:
			hist = (hist1 * 0x62) - (hist2 * 0x37);
			break;
		//default:
		//	hist = (q>>4)*hist1+(q>>4)*hist2; // a bit weird but it's in the code, never used
	}
	hist=(hist+0x20)>>6;
	if (hist >  0x1fffff) hist= 0x1fffff;
	if (hist < -0x200000) hist=-0x200000;

	cur = ( ( (short)(bits << 12) >> (q & 0xf)) << 6) + hist;
	
	*hist2p = *hist1p;
	*hist1p = cur;

	cur>>=6;

	if ( cur < -0x8000 ) return -0x8000;
	if ( cur >  0x7fff ) return  0x7fff;

	return (short)cur;
}

// decode 32 bytes of input (28 samples), assume stereo
int ADPdecodebuffer(unsigned char *input, short *outl, short * outr, long *histl1, long *histl2, long *histr1, long *histr2) {
	int i;
	for( i = 0; i < SAMPLES_PER_BLOCK; i++ )
	{
		outl[i] = ADPDecodeSample( input[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] & 0xf, input[0], histl1, histl2 );
		outr[i] = ADPDecodeSample( input[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] >> 4, input[1], histr1, histr2 );
	}
	return 0;
}
