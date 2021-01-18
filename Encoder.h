#pragma once

#include <cassert>
#include <cstring>
#include <climits>
#include <cstdint>

#include <iostream>

#include <CodecCommon.h>
#include <codec_api.h>

namespace codec {

class Encoder {
public:
    Encoder();
    ~Encoder();
    void start();
    bool isStarted() const;
    void encodeFrame (YUVFrame* frame);
    void flushFrame();
    void forceIntra();
    virtual void setParameter(SEncParamExt& param);
    virtual void onEncodedStream(uint8_t* bitstream, int32_t len, uint64_t ts, FrameType type, uint32_t temporalId);
protected:
    ISVCEncoder* venc;
    SEncParamExt encParam;
    bool started;

    void handleBsInfo(SFrameBSInfo& info);
    bool isExtParam() const;

    static Mapper<EVideoFrameType,FrameType> frameTypeMap;
};

};
