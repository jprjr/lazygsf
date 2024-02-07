#include "lazygsf.h"

#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>
#include <stdlib.h>
#include <string.h>


/* buffer size in PCM frames */
#define LAZYGSF_BUFFER_SIZE 2048

#define LAZYGSF_MIN(a,b) ( (a) < (b) ? (a) : (b) )

struct gsf_state {
    /* include mgba's mAVStream struct and expand it with our needs */
    struct mAVStream stream;

    /* the mgba core */
    struct mCore *core;

    /* the ROM data */
    uint8_t *rom;
    size_t   rom_size;

    /* samples buffer */
    int16_t samples[LAZYGSF_BUFFER_SIZE * 2];
    unsigned int buffered;

    unsigned int sample_rate;
    unsigned int flag; /* gets set once decoding has begun */
};

static uint32_t
GSF_unpack_uint32le(const uint8_t *b) {
    return (((uint32_t)b[3])<<24) |
           (((uint32_t)b[2])<<16) |
           (((uint32_t)b[1])<<8 ) |
           (((uint32_t)b[0])    );
}

static void
GSF_postAudioBuffer(struct mAVStream * stream, blip_t * left, blip_t * right) {
    gsf_state_t *state = (gsf_state_t *) stream;
    /* blip_read_samples accepts a stereo flag, that writes out to every-other-sample */
    blip_read_samples(left, state->samples, LAZYGSF_BUFFER_SIZE, true);
    blip_read_samples(right, state->samples + 1, LAZYGSF_BUFFER_SIZE, true);
    state->buffered = LAZYGSF_BUFFER_SIZE;
}

static void
GSF_nulllog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args)
{
    (void)logger;
    (void)category;
    (void)level;
    (void)format;
    (void)args;
}

static struct mLogger GSF_null_logger = {
    .log = GSF_nulllog,
};

static int
GSF_load_core(gsf_state_t *state) {
    struct VFile *rom = NULL;
    rom = VFileFromConstMemory(state->rom, state->rom_size);
    if(!rom) {
        return 1;
    }

    state->core = mCoreFindVF(rom);
    if(!state->core) {
        return 1;
    }

    if(!state->sample_rate) {
        state->sample_rate = 44100;
    }

    state->stream.postAudioBuffer = GSF_postAudioBuffer;

    state->core->init(state->core);
    state->core->setAVStream(state->core, &state->stream);
    mCoreInitConfig(state->core,NULL);
    state->core->setAudioBufferSize(state->core,LAZYGSF_BUFFER_SIZE);

    blip_set_rates(state->core->getAudioChannel(state->core, 0), state->core->frequency(state->core), state->sample_rate);
    blip_set_rates(state->core->getAudioChannel(state->core, 1), state->core->frequency(state->core), state->sample_rate);

    struct mCoreOptions opts = {
        .skipBios = true,
        .useBios = false,
        .sampleRate = state->sample_rate,
        .volume = 0x100,
    };

    mCoreConfigLoadDefaults(&state->core->config, &opts);

    state->core->loadROM(state->core, rom);
    state->core->reset(state->core);

    return 0;
}

LAZYGSF_EXPORT void
gsf_init(void) {
    mLogSetDefaultLogger(&GSF_null_logger);
}

LAZYGSF_EXPORT
size_t gsf_get_state_size(void) {
    return sizeof(gsf_state_t);
}

LAZYGSF_EXPORT
void gsf_clear(gsf_state_t *state) {
    memset(state,0,sizeof(gsf_state_t));
}

LAZYGSF_EXPORT unsigned int
gsf_get_sample_rate(gsf_state_t *state) {
    return state->sample_rate;
}

LAZYGSF_EXPORT unsigned int
gsf_set_sample_rate(gsf_state_t *state, unsigned int sample_rate) {
    if(state->flag) return state->sample_rate;

    state->sample_rate = sample_rate;

    if(state->core != NULL) {
        blip_set_rates(state->core->getAudioChannel(state->core, 0), state->core->frequency(state->core), state->sample_rate);
        blip_set_rates(state->core->getAudioChannel(state->core, 1), state->core->frequency(state->core), state->sample_rate);

        struct mCoreOptions opts = {
            .skipBios = true,
            .useBios = false,
            .sampleRate = state->sample_rate,
            .volume = 0x100,
        };

        mCoreConfigLoadDefaults(&state->core->config, &opts);

        state->core->reset(state->core);
    }
    return state->sample_rate;
}

LAZYGSF_EXPORT
int gsf_upload_section(gsf_state_t *state, const uint8_t *data, size_t size) {
/* according to gsf spec:
Offset         Size    Description
0x0000000      4       GSF_Entry_Point
0x0000004      4       GSF_Offset
0x0000008      4       Size of Rom.
0x000000C      XX      The Rom data itself.

The valid GSF_Entry_Points are 0x2000000 (Multiboot region), and 0x8000000 (Rom region).
The The High byte of the Offset should match the high byte of Entry_Point.

*/
    uint32_t entry   = 0;
    uint32_t offset  = 0;
    uint32_t rounded = 1;
    uint8_t *rom     = NULL;

    if(size < 0x0C) return -1;

    entry  = GSF_unpack_uint32le(&data[0x00]);
    offset = GSF_unpack_uint32le(&data[0x04]);
    size   = GSF_unpack_uint32le(&data[0x08]);

    offset -= entry;
    while(rounded < (offset + size)) {
        rounded <<= 1;
    }

    if( rounded > state->rom_size) {
        rom = realloc(state->rom, rounded);
        if(rom == NULL) return -1;
        if(state->rom == NULL) memset(rom, 0, rounded);
        state->rom = rom;
        state->rom_size = rounded;
    }

    memcpy(&state->rom[offset],&data[0x0C],size);
    return 0;
}

LAZYGSF_EXPORT int
gsf_render(gsf_state_t *state, int16_t *buf, size_t count) {
    size_t r = 0;
    size_t n = 0;

    if(state->core == NULL) {
        if(GSF_load_core(state)) return 1;
    }

    state->flag = 1;
    while(r < count) {
        while(state->buffered == 0) {
            state->core->runFrame(state->core);
        }

        n = LAZYGSF_MIN( (count - r) , state->buffered );
        if(buf != NULL) memcpy(&buf[r * 2], &state->samples[(LAZYGSF_BUFFER_SIZE - state->buffered) * 2], n * sizeof(int16_t) * 2);
        r += n;
        state->buffered -= n;
    }
    return 0;
}


LAZYGSF_EXPORT void
gsf_restart(gsf_state_t *state) {
    state->core->reset(state->core);
    state->buffered = 0;
    state->flag = 0;
    blip_clear(state->core->getAudioChannel(state->core, 0));
    blip_clear(state->core->getAudioChannel(state->core, 1));
}

LAZYGSF_EXPORT void
gsf_shutdown(gsf_state_t *state) {
    if(state->core) {
        state->core->unloadROM(state->core);
        mCoreConfigDeinit(&state->core->config);
        state->core->deinit(state->core);
        state->core = NULL;
    }
    if(state->rom) {
        free(state->rom);
        state->rom = NULL;
    }
}

/* mgba looks for projectName and projectVersion variables, we won't use them
 * but we need them to link */

const char* projectName = "lazygsf";
const char* projectVersion = "0";
