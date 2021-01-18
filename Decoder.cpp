#include <Decoder.h>

using namespace codec;

Decoder::Decoder() : vdec(nullptr) {
    long result = WelsCreateDecoder (&vdec);
    assert (result == 0 && vdec != nullptr);

    SDecodingParam decParam;
    memset (&decParam, 0, sizeof (SDecodingParam));
    decParam.uiTargetDqLayer = UCHAR_MAX;
    decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    result = vdec->Initialize (&decParam);
    assert (result == 0);

    bitstreamError = (dsRefLost | dsBitstreamError | dsDepLayerLost | dsDataErrorConcealed | dsRefListNullPtrs);
}
Decoder::~Decoder() {
    if (vdec) {
        vdec->Uninitialize();
        WelsDestroyDecoder (vdec);
        vdec = nullptr;
    }
}
void Decoder::decodeFrame (const uint8_t* bitstream, uint32_t len) {
    SBufferInfo bufInfo;
    memset (&bufInfo, 0, sizeof (SBufferInfo));

    uint8_t* data[3];
    memset (data, 0, sizeof (data));

    if (bitstream) {
        uint8_t nalu_type = 0;
        if (bitstream[2] == 1) nalu_type = bitstream[3];
        if (bitstream[3] == 1) nalu_type = bitstream[4];
        if (nalu_type) {
            std::cout << "[decodeFrame] lenght: " << std::dec << len
                << ", nalu_type: " << std::hex
                << (uint32_t) (nalu_type & 0x1f)
                << std::endl;
        }
    }
    DECODING_STATE result = vdec->DecodeFrame2 (bitstream, len, data, &bufInfo);

    if (bufInfo.iBufferStatus == 1) {
        YUVFrame frame = {
            {
                bufInfo.UsrData.sSystemBuffer.iWidth,
                bufInfo.UsrData.sSystemBuffer.iHeight,
                bufInfo.UsrData.sSystemBuffer.iStride[0],
                data[0]
            }, {
                bufInfo.UsrData.sSystemBuffer.iWidth/2,
                bufInfo.UsrData.sSystemBuffer.iHeight/2,
                bufInfo.UsrData.sSystemBuffer.iStride[1],
                data[1]
            }, {
                bufInfo.UsrData.sSystemBuffer.iWidth/2,
                bufInfo.UsrData.sSystemBuffer.iHeight/2,
                bufInfo.UsrData.sSystemBuffer.iStride[2],
                data[2]
            },
            bufInfo.uiOutYuvTimeStamp
        };
        onDecodedYuvFrame(frame);
    }
    if ((result & bitstreamError) != 0) {
        std::cout << "bitstream error " << std::hex << result << std::dec << ", ts: " << bufInfo.uiInBsTimeStamp << std::endl;
        onBitstreamError(result, bufInfo.uiInBsTimeStamp);
    }
}
void Decoder::flushFrame() {
    SBufferInfo bufInfo;
    memset (&bufInfo, 0, sizeof (SBufferInfo));

    uint8_t* data[3];
    memset (data, 0, sizeof (data));

    DECODING_STATE result = vdec->FlushFrame (data, &bufInfo);

    if (bufInfo.iBufferStatus == 1) {
        YUVFrame frame = {
            {
                bufInfo.UsrData.sSystemBuffer.iWidth,
                bufInfo.UsrData.sSystemBuffer.iHeight,
                bufInfo.UsrData.sSystemBuffer.iStride[0],
                data[0]
            }, {
                bufInfo.UsrData.sSystemBuffer.iWidth/2,
                bufInfo.UsrData.sSystemBuffer.iHeight/2,
                bufInfo.UsrData.sSystemBuffer.iStride[1],
                data[1]
            }, {
                bufInfo.UsrData.sSystemBuffer.iWidth/2,
                bufInfo.UsrData.sSystemBuffer.iHeight/2,
                bufInfo.UsrData.sSystemBuffer.iStride[2],
                data[2]
            },
            bufInfo.uiOutYuvTimeStamp
        };
        onDecodedYuvFrame(frame);
    }
    if ((result & bitstreamError) != 0) {
        std::cout << "bitstream error " << std::hex << result << std::dec << ", ts: " << bufInfo.uiInBsTimeStamp << std::endl;
        onBitstreamError(result, bufInfo.uiInBsTimeStamp);
    }
}

// virtual function
void Decoder::onDecodedYuvFrame(YUVFrame& frm) {}
void Decoder::onBitstreamError(int64_t what, uint64_t ts) {}
