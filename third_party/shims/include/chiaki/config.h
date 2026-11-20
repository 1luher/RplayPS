// Static replacement for chiaki-ng lib/config.h.in (CMake-configured upstream).
// Remote Play audio is decoded with Opus (third_party/opus); video decoders stay off.
#ifndef CHIAKI_CONFIG_H
#define CHIAKI_CONFIG_H
#define CHIAKI_LIB_ENABLE_OPUS 1
#define CHIAKI_LIB_ENABLE_PI_DECODER 0
#define CHIAKI_LIB_ENABLE_FFMPEG_DECODER 0
#endif
