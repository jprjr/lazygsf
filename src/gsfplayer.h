#ifndef GSFPLAYER_H
#define GSFPLAYER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GSFPlayer GSFPlayer;

/* init, should be called before any other function */
void
GSFPlayer_init(void);

/* open a new GSFPlayer for a file */
GSFPlayer *
GSFPlayer_open(const char *path);

/* returns the total number of tags in the file */
unsigned int
GSFPlayer_get_tag_total(GSFPlayer *);

/* returns a key, value pair for a given tag index */
const char *
GSFPlayer_get_tag(GSFPlayer *, const char *key);

/* returns a pointer to a NULL-terminated tag list as key/value pairs */
const char * const *
GSFPlayer_get_tags(GSFPlayer *);

/* returns the length of the song, in milliseconds */
unsigned int
GSFPlayer_get_length(GSFPlayer *);

/* returns the fade length of the song, in milliseconds */
unsigned int
GSFPlayer_get_fade(GSFPlayer *);

/* returns the current sample rate, defaults to 44100 */
unsigned int
GSFPlayer_get_sample_rate(GSFPlayer *);

/* sets the playback sample rate, returns the new sample rate on success. */
/* returns on old sample on failure. */
unsigned int
GSFPlayer_set_sample_rate(GSFPlayer *, unsigned int);

/* render PCM frames (16-bit, signed, 2 channels, interleaved) to a buffer,
 * returns number of frames rendered */
unsigned int
GSFPlayer_render(GSFPlayer *, int16_t *buf, unsigned int count);

/* restart song playback from the beginning */
void
GSFPlayer_reset(GSFPlayer *);

/* unload and delete resources */
void
GSFPlayer_close(GSFPlayer *);

#ifdef __cplusplus
}
#endif

#endif
