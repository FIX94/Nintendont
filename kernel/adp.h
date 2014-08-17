/*

  in_cube Gamecube Stream Player for Winamp
  by hcs

  includes work by Destop and bero

*/

#ifndef _ADP_H_
#define _ADP_H_

// DLS (a.k.a. DTK, TRK, ADP)
// uses same algorithm as XA, apparently
// ADP decoder function by hcs, reversed from dtkmake (trkmake v1.4)

// decode 32 bytes of input (28 samples), assume stereo
int ADPdecodebuffer(unsigned char *input, short *outl, short * outr, long *histl1, long *histl2, long *histr1, long *histr2);

#endif
