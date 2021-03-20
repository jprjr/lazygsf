#ifndef LAZYGSF_H
#define LAZYGSF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gsf_state gsf_state_t;

/* init, should be called before any other function */
void
gsf_init(void);

/* Get the size of gsf_state_t */
size_t
gsf_get_state_size(void);

/* initializes an allocated gsf state */
void
gsf_clear(gsf_state_t *state);

/* sets the sample rate of the gsf, must be called before rendering */
unsigned int
gsf_set_sample_rate(gsf_state_t *state, unsigned int sample_rate);

unsigned int
gsf_get_sample_rate(gsf_state_t *state);

/* upload a chunk of data into the gsf_state_t object */
int
gsf_upload_section(gsf_state_t *state, const uint8_t *data, size_t size);

int
gsf_render(gsf_state_t *state, int16_t *buffer, size_t count);

void
gsf_restart(gsf_state_t *state);

void
gsf_shutdown(gsf_state_t *state);

#if 0

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
#endif

#ifdef __cplusplus
}
#endif

#endif
