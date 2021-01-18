#pragma once

#include <cassert>
#include <cstring>
#include <climits>
#include <cstdint>

#include <iostream>

#include <CodecCommon.h>
#include <codec_api.h>

namespace codec {
class Decoder {
public:
    Decoder();
    ~Decoder();
    void decodeFrame (const uint8_t* bitstream, uint32_t len);
    void flushFrame();
    virtual void onDecodedYuvFrame(YUVFrame& frm);
    virtual void onBitstreamError(int64_t what, uint64_t ts);
protected:
    ISVCDecoder* vdec;
    uint32_t bitstreamError;
};
};
