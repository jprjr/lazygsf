# gsfplayer

A library for playing back GSF files with [mGBA](https://github.com/mgba-emu/mgba).

## Building

This should build with CMake, in the usual way.

Pass `-DBUILD_SHARED_LIBS` to build this as a shared library.

If you build a static library, be aware there's a lot of symbols,
you'll want to take some post-processing steps to limit symbol
visibility.

Both `llvm-objcopy` and `objcopy` should be able to hide symbols: `objcopy --localize-hidden libgsfplayer.a`.

## Usage

Before calling any other functions, you'll want to call
`GSFPlayer_init()`.

Call `GSFPlayer_open(path)` to allocate and return a new
`GSFPlayer *`. If you get `NULL`, something went wrong.

You can retrieve tags with `GSFPlayer_get_tags()`, this
returns a `const char * const *` that you can iterate over.

Alternatively you can retrieve individual tags with `GSFPlayer_get_tag`.

The tags should contain `length` and `fade` values, these
will automatically get pased. You can retrieve the
length and fade times, in milliseconds, with `GSFPlayer_get_length` and `GSFPlayer_get_fade`.

By default the sample rate is 44100Hz. You can change it
with `GSFPlayer_set_sample_rate`, which will return
the new sample rate.

Then call `GSFPlayer_render` until you've rendered
enough audio frames, then `GSFPlayer_close` to
free resources.


## Licensing

MIT. See `LICENSE`

Exceptions apply to:
  * mGBA ( MPL 2.0, details in the `mgba` folder).
