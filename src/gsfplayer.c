#include "gsfplayer.h"

#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>
#include <psflib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define EXPORT __declspec(dllexport)
#else
#include <strings.h>
#if __GNUC__ >= 4
#define EXPORT __attribute__ ((visibility("default")))
#else
#define EXPORT
#endif
#endif

/* buffer size in PCM frames */
#define GSFPLAYER_BUFFER_SIZE 2048

#define GSFPLAYER_MIN(a,b) ( (a) < (b) ? (a) : (b) )

struct GSFPlayer {
    /* include mgba's mAVStream struct and expand it with our needs */
    struct mAVStream stream;

    /* the mgba core */
    struct mCore *core;

    /* the ROM data */
    uint8_t *rom;

    /* samples buffer */
    int16_t samples[GSFPLAYER_BUFFER_SIZE * 2];
    unsigned int buffered;

    unsigned int length;
    unsigned int fade;

    /* tags */
    char **tag_data;
    unsigned int tags;

    unsigned int sample_rate;
    unsigned int flag; /* gets set once decoding has begun */
};

static const char * const gsfplayer_separators = "\\/:|";

static uint32_t
gsfplayer_unpack_uint32le(const uint8_t *b) {
    return (((uint32_t)b[3])<<24) |
           (((uint32_t)b[2])<<16) |
           (((uint32_t)b[1])<<8 ) |
           (((uint32_t)b[0])    );
}

/**
 * parses H:M:S into milliseconds
 */
static unsigned int
gsfplayer_parse_time(const char *ts)
{
	unsigned int i = 0;
	unsigned int t = 0;
	unsigned int c = 0;
	unsigned int m = 1000;
	for(i=0;i<strlen(ts);i++)
	{
		if(ts[i] == ':')
		{
			t *= 60;
			t += c*60;
			c = 0;
		}
		else if(ts[i] == '.') {
			m = 1;
			t += c;
			c = 0;
		}
		else
		{
			if(ts[i] < 48 || ts[i] > 57)
			{
				return 0;
			}
			c *= 10;
			c += (ts[i] - 48) * m;
		}
	}
	return c + t;
}

static void
gsfplayer_postAudioBuffer(struct mAVStream * stream, blip_t * left, blip_t * right) {
    GSFPlayer *player = (GSFPlayer *) stream;
    /* blip_read_samples accepts a stereo flag, that writes out to every-other-sample */
    blip_read_samples(left, player->samples, GSFPLAYER_BUFFER_SIZE, true);
    blip_read_samples(right, player->samples + 1, GSFPLAYER_BUFFER_SIZE, true);
    player->buffered = GSFPLAYER_BUFFER_SIZE;
}

static void *
gsfplayer_fopen(void *userdata, const char *filename) {
    (void)userdata;
    return fopen(filename,"rb");
}

static size_t
gsfplayer_fread(void *buffer, size_t size, size_t count, void *handle) {
    return fread(buffer,size,count,(FILE *)handle);
}

static int
gsfplayer_fseek(void *handle, int64_t offset, int whence) {
    return fseek((FILE *)handle,offset,whence);
}

static int
gsfplayer_fclose(void *handle) {
    return fclose((FILE *)handle);
}

static long
gsfplayer_ftell(void *handle) {
    return ftell((FILE *)handle);
}

/* strdup is non-standard so bring our own */
static char *
gsfplayer_strdup(const char *src) {
    size_t len = strlen(src);
    char *d = NULL;
    d = malloc(len+1);
    if(d != NULL) {
        memcpy(d,src,len+1);
    }
    return d;
}

static int
gsfplayer_tag_handler(void *ctx, const char *name, const char *value) {
    char **tag_data = NULL;
    GSFPlayer *player = (GSFPlayer *)ctx;

    if(strcasecmp(name,"length") == 0) {
        player->length = gsfplayer_parse_time(value);
    } else if(strcasecmp(name,"fade") == 0) {
        player->fade = gsfplayer_parse_time(value);
    }

    tag_data = (char **)realloc(player->tag_data, sizeof(char *) * ((player->tags + 2) * 2));
    if(tag_data == NULL) {
        free(player->tag_data);
        player->tag_data = NULL;
        return -1;
    }
    player->tag_data = tag_data;
    player->tag_data[(player->tags * 2) + 0] = gsfplayer_strdup(name);
    player->tag_data[(player->tags * 2) + 1] = gsfplayer_strdup(value);
    player->tag_data[(player->tags * 2) + 2] = NULL;
    player->tag_data[(player->tags * 2) + 3] = NULL;
    player->tags++;
    return 0;
}

static const psf_file_callbacks gsfplayer_psf_stdio = {
    gsfplayer_separators,
    NULL,
    gsfplayer_fopen,
    gsfplayer_fread,
    gsfplayer_fseek,
    gsfplayer_fclose,
    gsfplayer_ftell,
};

struct gsfplayer_loader_state {
    uint8_t *data;
    size_t data_size;
};

static int gsfplayer_loader(void * context, const uint8_t * exe, size_t exe_size,
                      const uint8_t * reserved, size_t reserved_size)
{
/* according to gsf spec:
Offset         Size    Description
0x0000000      4       GSF_Entry_Point
0x0000004      4       GSF_Offset
0x0000008      4       Size of Rom.
0x000000C      XX      The Rom data itself.

The valid GSF_Entry_Points are 0x2000000 (Multiboot region), and 0x8000000 (Rom region).
The The High byte of the Offset should match the high byte of Entry_Point.

*/
    struct gsfplayer_loader_state *rom = (struct gsfplayer_loader_state *)context;
    uint32_t entry  = 0;
    uint32_t offset = 0;
    uint32_t size   = 0;
    uint32_t rounded = 1;
    uint8_t *data   = NULL;

    (void)reserved;
    (void)reserved_size;

    if(exe_size < 0x0C) return -1;

    entry  = gsfplayer_unpack_uint32le(&exe[0x00]);
    offset = gsfplayer_unpack_uint32le(&exe[0x04]);
    size   = gsfplayer_unpack_uint32le(&exe[0x08]);

    offset -= entry;
    while(rounded < (offset + size)) {
        rounded <<= 1;
    }

    if( rounded > rom->data_size) {
        data = realloc(rom->data, rounded);
        if(data == NULL) return -1;
        if(rom->data == NULL) memset(data, 0, rounded);
        rom->data = data;
        rom->data_size = rounded;
    }

    memcpy(&rom->data[offset],&exe[0x0C],size);
    return 0;
}

static void gsfplayer_nulllog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args)
{
    (void)logger;
    (void)category;
    (void)level;
    (void)format;
    (void)args;
}

static struct mLogger gsfplayer_null_logger = {
    .log = gsfplayer_nulllog,
};

EXPORT GSFPlayer *
GSFPlayer_open(const char *path) {
    struct gsfplayer_loader_state state;
    GSFPlayer *player = NULL;
    struct VFile *rom = NULL;

    player = malloc(sizeof(GSFPlayer));
    if(player == NULL) return NULL;
    memset(player,0,sizeof(GSFPlayer));

    player->sample_rate = 44100;

    memset(&state,0,sizeof(struct gsfplayer_loader_state));

    if(psf_load(path,
        &gsfplayer_psf_stdio,0x22,gsfplayer_loader,&state,
        gsfplayer_tag_handler,player,0,
        NULL,NULL) <= 0) {
        GSFPlayer_close(player);
        return NULL;
    }

    player->rom = state.data;

    rom = VFileFromConstMemory(player->rom, state.data_size);
    if(!rom) {
        GSFPlayer_close(player);
        return NULL;
    }

    player->core = mCoreFindVF(rom);
    if(!player->core) {
        GSFPlayer_close(player);
        return NULL;
    }

    player->rom = state.data;

    player->stream.postAudioBuffer = gsfplayer_postAudioBuffer;

    player->core->init(player->core);
    player->core->setAVStream(player->core, &player->stream);
    mCoreInitConfig(player->core,NULL);
    player->core->setAudioBufferSize(player->core,GSFPLAYER_BUFFER_SIZE);

    blip_set_rates(player->core->getAudioChannel(player->core, 0), player->core->frequency(player->core), player->sample_rate);
    blip_set_rates(player->core->getAudioChannel(player->core, 1), player->core->frequency(player->core), player->sample_rate);

    struct mCoreOptions opts = {
        .skipBios = true,
        .useBios = false,
        .sampleRate = player->sample_rate,
        .volume = 0x100,
    };

    mCoreConfigLoadDefaults(&player->core->config, &opts);

    player->core->loadROM(player->core, rom);
    player->core->reset(player->core);

    return player;
}

EXPORT unsigned int
GSFPlayer_render(GSFPlayer *player, int16_t *buf, unsigned int count) {
    unsigned int r = 0;
    unsigned int n = 0;
    player->flag = 1;
    while(r < count) {
        while(player->buffered == 0) {
            player->core->runFrame(player->core);
        }

        n = GSFPLAYER_MIN( (count - r) , player->buffered );
        if(buf != NULL) memcpy(&buf[r * 2], &player->samples[(GSFPLAYER_BUFFER_SIZE - player->buffered) * 2], n * sizeof(int16_t) * 2);
        r += n;
        player->buffered -= n;
    }
    return r;
}

EXPORT unsigned int
GSFPlayer_get_sample_rate(GSFPlayer *player) {
    return player->sample_rate;
}

EXPORT unsigned int
GSFPlayer_set_sample_rate(GSFPlayer *player, unsigned int sample_rate) {
    if(player->flag) return player->sample_rate;

    player->sample_rate = sample_rate;

    blip_set_rates(player->core->getAudioChannel(player->core, 0), player->core->frequency(player->core), player->sample_rate);
    blip_set_rates(player->core->getAudioChannel(player->core, 1), player->core->frequency(player->core), player->sample_rate);

    struct mCoreOptions opts = {
        .skipBios = true,
        .useBios = false,
        .sampleRate = player->sample_rate,
        .volume = 0x100,
    };

    mCoreConfigLoadDefaults(&player->core->config, &opts);

    player->core->reset(player->core);
    return player->sample_rate;
}

EXPORT void
GSFPlayer_reset(GSFPlayer *player) {
    player->core->reset(player->core);
    player->buffered = 0;
    player->flag = 0;
    blip_clear(player->core->getAudioChannel(player->core, 0));
    blip_clear(player->core->getAudioChannel(player->core, 1));
}

EXPORT unsigned int
GSFPlayer_get_length(GSFPlayer *player) {
    return player->length;
}

EXPORT unsigned int
GSFPlayer_get_fade(GSFPlayer *player) {
    return player->fade;
}

EXPORT unsigned int
GSFPlayer_get_tag_total(GSFPlayer *player) {
    return player->tags;
}

EXPORT const char *
GSFPlayer_get_tag(GSFPlayer *player, const char *key) {
    unsigned int i = 0;
    const char *value = NULL;
    while(i < player->tags) {
        if(strcasecmp(key,player->tag_data[(i*2)]) == 0) {
            value = player->tag_data[(i*2) + 1];
            break;
        }
        i++;
    }
    return value;
}

EXPORT const char * const *
GSFPlayer_get_tags(GSFPlayer *player) {
    return (const char * const *)player->tag_data;
}

EXPORT void
GSFPlayer_close(GSFPlayer *player) {
    unsigned int i = 0;
    if(player->tag_data) {
        for(i=0;i<player->tags * 2; i++) {
            free(player->tag_data[i]);
        }
        free(player->tag_data);
        player->tag_data = NULL;
    }

    if(player->core) {
        player->core->unloadROM(player->core);
        mCoreConfigDeinit(&player->core->config);
        player->core->deinit(player->core);
        player->core = NULL;
    }
    if(player->rom) {
        free(player->rom);
        player->rom = NULL;
    }
    free(player);
}


EXPORT void
GSFPlayer_init(void) {
    mLogSetDefaultLogger(&gsfplayer_null_logger);
}


