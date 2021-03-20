#ifndef LAZYGSF_H
#define LAZYGSF_H

#include <stdint.h>
#include <stddef.h>


#if __GNUC__ >= 4
  #define LAZYGSF_EXPORT __attribute__ ((visibility ("default")))
#else
  #define LAZYGSF_EXPORT
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef struct gsf_state gsf_state_t;

/* init, should be called before any other function */
LAZYGSF_EXPORT void
gsf_init(void);

/* Get the size of gsf_state_t */
LAZYGSF_EXPORT size_t
gsf_get_state_size(void);

/* initializes an allocated gsf state */
LAZYGSF_EXPORT void
gsf_clear(gsf_state_t *state);

/* sets the sample rate of the gsf, must be called before rendering */
LAZYGSF_EXPORT unsigned int
gsf_set_sample_rate(gsf_state_t *state, unsigned int sample_rate);

LAZYGSF_EXPORT unsigned int
gsf_get_sample_rate(gsf_state_t *state);

/* upload a chunk of data into the gsf_state_t object */
LAZYGSF_EXPORT int
gsf_upload_section(gsf_state_t *state, const uint8_t *data, size_t size);

LAZYGSF_EXPORT int
gsf_render(gsf_state_t *state, int16_t *buffer, size_t count);

LAZYGSF_EXPORT void
gsf_restart(gsf_state_t *state);

LAZYGSF_EXPORT void
gsf_shutdown(gsf_state_t *state);


#ifdef __cplusplus
}
#endif

#endif
