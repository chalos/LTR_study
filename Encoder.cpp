#include <Encoder.h>

using namespace codec;

Encoder::Encoder() : venc(nullptr), started(false) {
    long result = WelsCreateSVCEncoder(&venc);
    assert (result == 0 && venc != nullptr);

    result = venc->GetDefaultParams(&encParam);
    assert (result == 0);
}
Encoder::~Encoder() {
    if (venc) {
        venc->Uninitialize();
        WelsDestroySVCEncoder (venc);
        venc = nullptr;
    }
}

void Encoder::start() {
    setParameter(encParam);
    if (!isExtParam()) {
        std::cout << "use base initailze" << std::endl;
        SEncParamBase param;
        memset (&param, 0, sizeof (SEncParamBase));
        param.iUsageType     = encParam.iUsageType;
        param.fMaxFrameRate  = encParam.fMaxFrameRate;
        param.iPicWidth      = encParam.iPicWidth;
        param.iPicHeight     = encParam.iPicHeight;
        param.iTargetBitrate = encParam.iTargetBitrate;
        long result = venc->Initialize (&param);
        assert (result == 0);
    } else {
        std::cout << "use ext initailze" << std::endl;
        long result = venc->InitializeExt (&encParam);
        assert (result == 0);
    }
    started = true;
}

bool Encoder::isStarted() const {
    return started;
}

void Encoder::encodeFrame (YUVFrame* frame) {
    if (!isStarted()) start();
    assert (isStarted());

    SFrameBSInfo info;
    memset (&info, 0, sizeof(SFrameBSInfo));

    if (frame == nullptr) {
        return flushFrame();
    }

    // frame width height is same as frame width height
    assert (frame->y.width == encParam.iPicWidth && frame->y.height == encParam.iPicHeight);
    // chroma width height is half of luma's
    assert (frame->u.width == encParam.iPicWidth/2 && frame->u.height == encParam.iPicHeight/2);
    assert (frame->u.width == frame->v.width && frame->u.height == frame->v.height);
    // plane stride must >= to width
    assert (frame->y.stride >= frame->y.width && frame->u.stride >= frame->u.width && frame->v.stride >= frame->v.width);

    SSourcePicture pic;
    memset (&pic, 0, sizeof(SSourcePicture));
    pic.iPicWidth = encParam.iPicWidth;
    pic.iPicHeight = encParam.iPicHeight;
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = frame->y.stride;
    pic.iStride[1] = frame->u.stride;
    pic.iStride[2] = frame->v.stride;
    pic.pData[0] = frame->y.data;
    pic.pData[1] = frame->u.data;
    pic.pData[2] = frame->v.data;

    venc->EncodeFrame (&pic, &info);
    handleBsInfo(info);
}
void Encoder::flushFrame() {
    if (venc == nullptr || !started) return;

    SFrameBSInfo info;
    memset (&info, 0, sizeof(SFrameBSInfo));
    venc->EncodeFrame(nullptr, &info);
    handleBsInfo(info);
}
void Encoder::forceIntra() {
    if (venc == nullptr || !started) return;

    venc->ForceIntraFrame(true);
}

void Encoder::handleBsInfo(SFrameBSInfo& info) {
    if (info.eFrameType == videoFrameTypeSkip) return;
    FrameType ft = INVALID;
    if (!frameTypeMap.loopup(info.eFrameType, &ft)) return;

    uint32_t len = 0;
    for (int i=0; i<info.iLayerNum; i++) {
        const SLayerBSInfo& layerInfo = info.sLayerInfo[i];
        for (int j = 0; j<layerInfo.iNalCount; j++) {
            len += layerInfo.pNalLengthInByte[j];
        }
    }

    std::cout << std::dec << "[encodeFrame] "
        << "length: " << len
        << ", layer num: " << info.iLayerNum
        << ", frame type: " << info.eFrameType
        << ", { temporal id: " << (uint32_t) info.sLayerInfo[0].uiTemporalId
        << ", spatial id: " << (uint32_t) info.sLayerInfo[0].uiSpatialId
        << ", quality id: " << (uint32_t) info.sLayerInfo[0].uiQualityId
        << ", frame type: " << (uint32_t) info.sLayerInfo[0].eFrameType
        << ", layer type: " << (uint32_t) info.sLayerInfo[0].uiLayerType
        << " }" << std::endl;

    onEncodedStream (info.sLayerInfo[0].pBsBuf,
        len, info.uiTimeStamp, ft, info.sLayerInfo[0].uiTemporalId);
}

bool Encoder::isExtParam() const {
    return !(SM_SINGLE_SLICE == encParam.sSpatialLayers[0].sSliceArgument.uiSliceMode
        && !encParam.bEnableDenoise
        && encParam.iSpatialLayerNum == 1
        && !encParam.bIsLosslessLink
        && !encParam.bEnableLongTermReference
        && !encParam.iEntropyCodingModeFlag);
}

// virtual function
void Encoder::setParameter(SEncParamExt& param) {}
void Encoder::onEncodedStream(uint8_t* bitstream, int32_t len, uint64_t ts, FrameType type, uint32_t temporalId){}

// static declaration
Mapper<EVideoFrameType,FrameType> Encoder::frameTypeMap = {
    {videoFrameTypeInvalid, INVALID},
    {videoFrameTypeIDR, IDRFrame},
    {videoFrameTypeI, IFrame},
    {videoFrameTypeP, PFrame},
};
