# gsfplayer

A library for playing back GSF files with [mGBA](https://github.com/mgba-emu/mgba).

## Building

This should build with CMake, in the usual way.

Pass `-DBUILD_SHARED_LIBS` to build this as a shared library.

If you build a static library, be aware there's a lot of symbols,
you'll want to take some post-processing steps to limit symbol
visibility.

Both `llvm-objcopy` and `objcopy` should be able to hide symbols: `objcopy --localize-hidden libgsfplayer.a`.


## Licensing

MIT. See `LICENSE`

Exceptions apply to:
  * mGBA ( MPL 2.0, details in the `mgba` folder).
