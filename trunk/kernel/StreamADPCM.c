// Adapted from in_cube by hcs & destop

#include "StreamADPCM.h"

#ifdef AUDIOSTREAM
// Convert Gamecube DTK/TRK/ADP tracks (essentially CD-XA ADPCM) to the
// nearest equivalent "DSP" ADPCM.

void transcode_frame(const char * framebuf, int channel, char * outframe)
{
    int chanshift = (channel == 0 ? 0 : 4);
    uint8_t adp_frame_header = framebuf[0 + channel];
    int scale_log = 12 - (adp_frame_header & 0xf);
    int predictor = adp_frame_header >> 4;

 //   CHECK_ERROR(scale_log < 0 || scale_log > 16, "scale range");
    uint8_t dsp_frame_header = scale_log | (predictor << 4);

 //   uint8_t outframe[16];
    outframe[0] = outframe[8] = dsp_frame_header;

	int i,j,k;

    for (j = 0, k = 4; j < 2; j++) {
        for (i = 0; i < 14; i += 2, k += 2) {
            uint8_t sample = (framebuf[k] >> chanshift) & 0xf;
            uint8_t b = sample << 4;

            sample = (framebuf[k+1] >> chanshift) & 0xf;
            b |= sample;

            outframe[j*8 + 1 + i/2] = b;
        }
    }
}


s32 adpcm_history1_32;
s32 adpcm_history2_32;

/* signed nibbles come up a lot */
static int nibble_to_int[16] = {0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1};

static inline int get_high_nibble_signed(u8 n) {
    /*return ((n&0x70)-(n&0x80))>>4;*/
    return nibble_to_int[n>>4];
}

static inline int get_low_nibble_signed(u8 n) {
    /*return (n&7)-(n&8);*/
    return nibble_to_int[n&0xf];
}

static inline int clamp16(s32 val) {
        if (val>32767) return 32767;
            if (val<-32768) return -32768;
                return val;
}
void decode_ngc_dtk( u8 *stream, u16 * outbuf, int channelspacing, s32 first_sample, s32 samples_to_do, int channel)
{
    int i=first_sample;
    s32 sample_count;

    int framesin = first_sample/28;

    u8 q = stream[framesin*32+channel];
    s32 hist1 = 0;
    s32 hist2 = 0;

    first_sample = first_sample%28;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing)
	{
		int sample_byte = stream[framesin*32+4+i];

        s32 hist=0;

        switch (q>>4)
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
        }

        hist = (hist+0x20)>>6;
        if (hist >  0x1fffff) hist =  0x1fffff;
        if (hist < -0x200000) hist = -0x200000;

        hist2 = hist1;

        hist1 = ((((channel==0?
                    get_low_nibble_signed(sample_byte):
                    get_high_nibble_signed(sample_byte)
                   ) << 12) >> (q & 0xf)) << 6) + hist;

        outbuf[sample_count] = clamp16(hist1 >> 6);
    }

    adpcm_history1_32 = hist1;
    adpcm_history2_32 = hist2;
}
#endif
