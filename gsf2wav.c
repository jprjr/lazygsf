#include "src/gsfplayer.h"

#include <stdio.h>
#include <stdint.h>

/* demo program that renders a GSF file into a WAV */

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 1024

int16_t buffer[BUFFER_SIZE * 2];
uint8_t packed[BUFFER_SIZE * 4];

/* global variable for the current pcm frame */
uint64_t frame_no = 0;

/* global variable - total pcm frames */
uint64_t frame_total = 0;

/* global variable - frame to start fading on */
uint64_t frame_fade = 0;

static int
write_wav_header(FILE *f);

static void
pack_frames(void);

static void
pack_int16le(uint8_t *d, int16_t n);

static void
pack_uint16le(uint8_t *d, uint16_t n);

static void
pack_uint32le(uint8_t *d, uint32_t n);

static void
apply_fade(void) {
    uint64_t i = 0;
    float fade;
    for(i=0;i<BUFFER_SIZE;i++) {
        if(frame_no + i > frame_total) {
            buffer[(i*2)+0] = 0;
            buffer[(i*2)+1] = 0;
        }
        if(frame_no + i > frame_fade) {
            fade = ((float)(frame_total - frame_no - i)) / ((float)(frame_total - frame_fade));
            fade *= fade;
            buffer[(i*2) + 0] *= fade;
            buffer[(i*2) + 1] *= fade;
        }
    }
}

int main(int argc, const char *argv[]) {
    const char * const *tags = NULL;
    FILE *out = NULL;

    if(argc < 3) {
        printf("Usage: %s /path/to/minigsf /path/to/output.wav\n",
        argv[0]);
        return 1;
    }

    GSFPlayer_init();
    GSFPlayer *player = GSFPlayer_open(argv[1]);
    if(player == NULL) {
        printf("error loading\n");
        return 1;
    }

    out = fopen(argv[2],"wb");
    if(out == NULL) {
        printf("error opening output wav\n");
        return 1;
    }

    if(GSFPlayer_set_sample_rate(player,SAMPLE_RATE) != SAMPLE_RATE) {
        printf("error setting sample rate\n");
        return 1;
    }


    tags = GSFPlayer_get_tags(player);
    while(tags && *tags) {
        printf("tag: %s=%s\n",tags[0],tags[1]);
        tags += 2;
    }

    frame_total = GSFPlayer_get_length(player);
    frame_fade = frame_total;
    frame_total += GSFPlayer_get_fade(player);

    frame_fade *= SAMPLE_RATE;
    frame_fade /= 1000;

    frame_total *= SAMPLE_RATE;
    frame_total /= 1000;

    write_wav_header(out);

    for(frame_no=0;frame_no<frame_total; frame_no += BUFFER_SIZE) {
        GSFPlayer_render(player,buffer,BUFFER_SIZE);
        apply_fade();
        pack_frames();
        fwrite(packed,4, BUFFER_SIZE, out);
    }

    GSFPlayer_close(player);

    fclose(out);

    return 0;
}

static int write_wav_header(FILE *f) {
    unsigned int data_size = (unsigned int)frame_total * 4;
    uint8_t tmp[4];
    if(fwrite("RIFF",1,4,f) != 4) return 1;
    pack_uint32le(tmp, 4 + ( 8 + data_size ) + (8 + 40) );
    if(fwrite(tmp,1,4,f) != 4) return 1;

    if(fwrite("WAVE",1,4,f) != 4) return 1;
    if(fwrite("fmt ",1,4,f) != 4) return 1;

    /* fmtSize
     * 16 = standard wave
     * 40 = extensible
     */
    pack_uint32le(tmp,16);
    if(fwrite(tmp,1,4,f) != 4) return 1;

    /* audioFormat:
     * 1 = PCM
     * 3 = float
     * 6 = alaw
     * 7 = ulaw
     * 0xfffe = extensible */
    pack_uint16le(tmp,1);
    if(fwrite(tmp,1,2,f) != 2) return 1;

    /* numChannels */
    pack_uint16le(tmp,2);
    if(fwrite(tmp,1,2,f) != 2) return 1;

    /* sampleRate */
    pack_uint32le(tmp,SAMPLE_RATE);
    if(fwrite(tmp,1,4,f) != 4) return 1;

    /* dataRate (bytes per second) */
    pack_uint32le(tmp,SAMPLE_RATE * 4);
    if(fwrite(tmp,1,4,f) != 4) return 1;

    /* block alignment (channels * sample size) */
    pack_uint16le(tmp,4);
    if(fwrite(tmp,1,2,f) != 2) return 1;

    /* bits per sample */
    pack_uint16le(tmp,16);
    if(fwrite(tmp,1,2,f) != 2) return 1;

    if(fwrite("data",1,4,f) != 4) return 1;

    pack_uint32le(tmp,data_size);
    if(fwrite(tmp,1,4,f) != 4) return 1;

    return 0;
}

static void pack_frames(void) {
    unsigned int i = 0;
    while(i < BUFFER_SIZE) {
        pack_int16le(&packed[(i*4)+0],buffer[(i*2)+0]);
        pack_int16le(&packed[(i*4)+2],buffer[(i*2)+1]);
        i++;
    }
}

static void pack_int16le(uint8_t *d, int16_t n) {
    d[0] = (uint8_t)((uint16_t) n      );
    d[1] = (uint8_t)((uint16_t) n >> 8 );
}

static void pack_uint16le(uint8_t *d, uint16_t n) {
    d[0] = (uint8_t)((uint16_t) n      );
    d[1] = (uint8_t)((uint16_t) n >> 8 );
}

static void pack_uint32le(uint8_t *d, uint32_t n) {
    d[0] = (uint8_t)(n      );
    d[1] = (uint8_t)(n >> 8 );
    d[2] = (uint8_t)(n >> 16);
    d[3] = (uint8_t)(n >> 24);
}
