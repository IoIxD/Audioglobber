#pragma once

#include "decoder.hpp"
#include "miniaudio.h"

void snd_callback(ma_device *device, void *output, const void *input,
                  ma_uint32 frame_count);
