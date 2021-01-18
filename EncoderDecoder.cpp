
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <memory>

#include <Decoder.h>
#include <Encoder.h>

using namespace codec;

class EncoderDecoder : public Encoder, public Decoder {
public:
    EncoderDecoder(uint32_t w=320, uint32_t h=240, uint32_t fc=120, RecoverType recover = IDR_RECOVER, uint32_t lr=0)
    : Encoder(), Decoder(), width(w), height(h), framecount(fc), mRecoverType(recover), lossRate(lr) {
        encParam.iPicWidth = width;
        encParam.iPicHeight = height;

        frameMemSize = width * height * 3 / 2;

        unsigned int uiTraceLevel = WELS_LOG_DETAIL;
        venc->SetOption (ENCODER_OPTION_TRACE_LEVEL, &uiTraceLevel);

        uiTraceLevel = WELS_LOG_DETAIL;
        vdec->SetOption (DECODER_OPTION_TRACE_LEVEL, &uiTraceLevel);
    }
    ~EncoderDecoder() {}
    void setRecover(RecoverType rp) {
        mRecoverType = rp;
    }

    void start() {
        decodedFrameCount = 0;
        errorFrameCount = 0;
        lastCorrectDecoded = -1;
        lastIdrPictureId = -1;

        contentSetup();
        YUVFrame frame = {
            { width, height, width, nullptr },
            { width/2, height/2, width/2, nullptr },
            { width/2, height/2, width/2, nullptr },
            0
        };
        for (int i=0; i<framecount; i++) {
            auto content = getContent(i);
            uint8_t* base = content.get();
            frame.y.data = base;
            frame.u.data = base + width * height;
            frame.v.data = frame.u.data + (width * height /4);
            frame.ts = i;

            encodeFrame(&frame);
        }
        //encodeFrame(nullptr);
        //Encoder::flushFrame();

        decodeFrame(nullptr, 0);
        Decoder::flushFrame();


        std::cout << "framecount: " << framecount
            << " vs "
            << " decodedFrameCount: " << decodedFrameCount
            << " + errorFrameCount: " << errorFrameCount
            << std::endl;

        assert (framecount <= decodedFrameCount+errorFrameCount);
        contentTearDown();
    }
    void setParameter(SEncParamExt& param) override {
        std::cout << "[DEBUG] setParameter" << std::endl;
        param.iUsageType = CAMERA_VIDEO_REAL_TIME;
        param.iTargetBitrate = 5000000;
        param.iMaxBitrate = UNSPECIFIED_BIT_RATE;
        param.fMaxFrameRate = 30;
        param.iRCMode = RC_OFF_MODE;
        param.bEnableDenoise = 1;
        param.iMultipleThreadIdc = 1; //single thread
        param.iSpatialLayerNum = 1;
        param.iTemporalLayerNum = 2;
        if (mRecoverType == LTR_RECOVER) {
            param.bEnableLongTermReference = 1;
            param.iLTRRefNum = 2; // only mark IDR
            param.iLtrMarkPeriod = 30; // mark to IDR
            param.eSpsPpsIdStrategy = INCREASING_ID; // every GOP ID increases during IDR
        }
        else {
            param.bEnableLongTermReference = 0;
        }
        param.bIsLosslessLink = 0;
        param.iEntropyCodingModeFlag = 0;
        param.bEnableDenoise = 0;
        param.iNumRefFrame = 1;
        for (int i=0; i<param.iSpatialLayerNum; i++) {
            param.sSpatialLayers[i].iVideoWidth = width >> (param.iSpatialLayerNum - i - 1);
            param.sSpatialLayers[i].iVideoHeight = height >> (param.iSpatialLayerNum - i - 1);
            param.sSpatialLayers[i].fFrameRate = 30;
            param.sSpatialLayers[i].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
            param.sSpatialLayers[i].sSliceArgument.uiSliceNum = 1;
            param.sSpatialLayers[i].sSliceArgument.uiSliceSizeConstraint = 0;
            param.sSpatialLayers[i].uiProfileIdc = PRO_BASELINE;
            param.sSpatialLayers[i].uiLevelIdc = LEVEL_4_1;
            param.sSpatialLayers[i].iMaxSpatialBitrate = 288000000;
        }

        SLTRConfig sLtrConfigVal;
        sLtrConfigVal.bEnableLongTermReference = param.bEnableLongTermReference;
        sLtrConfigVal.iLTRRefNum = param.iLTRRefNum;
        venc->SetOption(ENCODER_OPTION_LTR, &sLtrConfigVal);

        int32_t markPeriod = param.iLtrMarkPeriod;
        venc->SetOption(ENCODER_LTR_MARKING_PERIOD, &markPeriod);
    }
    void onEncodedStream(uint8_t* bitstream, int32_t len, uint64_t ts, FrameType type, uint32_t temporalId) override {
        //std::cout << "[onEncodedStream] len: " << len << ", ts: " << ts << ", type: " << type << std::endl;

        uint32_t rand_value = (std::rand() % 100)+1;
        if (ts >= 600 && rand_value <= lossRate && 0) {
            std::cout << "lost packet: "<< ts << std::endl;
            memset (bitstream, 0, len);
        }
        if (temporalId != 0) {
            errorFrameCount ++;
            return;
        }

        decodeFrame ((const uint8_t*)bitstream, len);

        SLTRMarkingFeedback mLtrMarkFeedback;
        mLtrMarkFeedback.uiFeedbackType = LTR_MARKING_SUCCESS;
        mLtrMarkFeedback.uiIDRPicId = lastIdrPictureId;
        mLtrMarkFeedback.iLTRFrameNum = lastMarkFrameNum;
        venc->SetOption(ENCODER_LTR_MARKING_FEEDBACK, &mLtrMarkFeedback);
    }
    void onDecodedYuvFrame(YUVFrame& frm) override {
        decodedFrameCount ++;

        int32_t temp = -1;
        vdec->GetOption (DECODER_OPTION_IDR_PIC_ID, &temp);
        if (temp != lastIdrPictureId) {
            lastCorrectDecoded = -1;
            lastIdrPictureId = temp;
        } else {
            vdec->GetOption (DECODER_OPTION_FRAME_NUM, &temp);
            lastCorrectDecoded = temp;
        }

        vdec->GetOption (DECODER_OPTION_LTR_MARKED_FRAME_NUM, &lastMarkFrameNum);

        int32_t frameNum;
        int32_t idrId;
        int32_t vcl_nal;
        int32_t temporal_id;
        int32_t marked;
        int32_t isRefPic;
        vdec->GetOption (DECODER_OPTION_FRAME_NUM, &frameNum);
        vdec->GetOption (DECODER_OPTION_IDR_PIC_ID, &idrId);
        vdec->GetOption (DECODER_OPTION_VCL_NAL , &vcl_nal);
        vdec->GetOption (DECODER_OPTION_TEMPORAL_ID, &temporal_id);
        vdec->GetOption (DECODER_OPTION_LTR_MARKING_FLAG, &marked);
        vdec->GetOption (DECODER_OPTION_IS_REF_PIC, &isRefPic);

        std::cout << "[DEBUG][dec] "
            << ", vcl_nal: " << std::dec << vcl_nal
            << ", temporal id: " << std::dec << temporal_id
            << ", iCurrentFrameNum: " << std::dec << frameNum
            << ", lastMarkFrameNum: " << std::dec << lastMarkFrameNum
            << ", lastCorrectDecoded: " << std::dec << lastCorrectDecoded
            << ", uiIDRPicId " << std::dec << idrId
            << ", marked " << std::dec << marked
            << ", isRefPic " << std::dec << isRefPic
            << std::endl;
    };
    void onBitstreamError(int64_t what, uint64_t ts) override {
        errorFrameCount ++;
        uint32_t option = 0; // 0: do nothing (should not handle), 1: Force IDR, 2: LTR

        SLTRRecoverRequest mLRR;

        if (mRecoverType == NOT_RECOVER) return;
        if (what & dsNoParamSets || mRecoverType == IDR_RECOVER) {
            // dsNoParamSets: only do IDR recovery in example
            // to-do: send PLI/SLI
            option = 1;
            std::cout << "[DEBUG][idr] error bs: " << std::hex << what << ", recover idr" << std::endl;
        } else if (mRecoverType == LTR_RECOVER) {
            option = 2;

            int32_t temporal_id = 0;
            vdec->GetOption (DECODER_OPTION_TEMPORAL_ID, &temporal_id);
            if(temporal_id == -1) mLRR.iLayerId  = 0;

            vdec->GetOption (DECODER_OPTION_FRAME_NUM, &mLRR.iCurrentFrameNum);
            vdec->GetOption (DECODER_OPTION_IDR_PIC_ID, &mLRR.uiIDRPicId);
            mLRR.iLastCorrectFrameNum = lastCorrectDecoded;
            mLRR.uiFeedbackType = LTR_RECOVERY_REQUEST;

            int32_t ltrMarkFlag, ltrMarkNum, vlcNal;
            vdec->GetOption (DECODER_OPTION_LTR_MARKING_FLAG, &ltrMarkFlag);
            vdec->GetOption (DECODER_OPTION_LTR_MARKED_FRAME_NUM, &ltrMarkNum);
            vdec->GetOption (DECODER_OPTION_VCL_NAL, &vlcNal);

            std::cout << "[DEBUG][ltr] error bs: " << std::hex << what
                << ", temporal id: " << std::dec << temporal_id
                << ", iCurrentFrameNum: " << std::dec << mLRR.iCurrentFrameNum
                << ", iLastCorrectFrameNum: " << std::dec << mLRR.iLastCorrectFrameNum
                << ", uiIDRPicId " << std::dec << mLRR.uiIDRPicId
                << ", ltrMarkFlag " << std::dec << ltrMarkFlag
                << ", ltrMarkNum " << std::dec << ltrMarkNum
                << ", vlcNal " << std::dec << vlcNal
                << std::endl;

            // to-do: send RPSI
        }

        if (option == 1) {
            Encoder::forceIntra();
        } else if (option == 2) {
            venc->SetOption (ENCODER_LTR_RECOVERY_REQUEST, &mLRR);
        }
    };

protected:
    virtual void contentSetup() {
        yuvContent = (uint8_t*)malloc(frameMemSize*8);
        for (int i=0; i<8; i++)
            generate_moving_block(width, height, i, yuvContent+(i*frameMemSize), 8);
    }

    virtual std::shared_ptr<uint8_t> getContent(uint32_t hint) {
        uint32_t loop_offset = hint % 8;
        uint8_t* base = yuvContent + (frameMemSize*loop_offset);
        return std::shared_ptr<uint8_t>(base, [](uint8_t* ptr){});
    }

    virtual void contentTearDown() {
        if (yuvContent) free (yuvContent);
    }

    int32_t width;
    int32_t height;
    uint32_t framecount;
    uint32_t frameMemSize;
    uint32_t decodedFrameCount;
    uint32_t errorFrameCount;
    uint8_t* yuvContent;

    uint32_t lossRate;

    int32_t lastCorrectDecoded;
    int32_t lastIdrPictureId;
    int32_t lastMarkFrameNum;

    RecoverType mRecoverType;
private:
    void generate_moving_block(uint32_t w, uint32_t h, uint32_t count, uint8_t* buffer, uint32_t loop) {
        uint8_t color1[3] = {0,0,0};
        uint8_t color2[3] = {255,255,255};

        uint32_t planesize[3] = {w * h, w * h/4, w * h/4};

        uint32_t thick = h / loop;
        uint32_t offset = thick * count;

        for (int i=0; i<h; i++) {
            for (int j=0; j<w; j++) {
                if (i >= offset && i<(offset+thick)) buffer[i * w + j] = color2[0];
                else buffer[i * w + j] = color1[0];
            }
        }
        for (int i=0; i<h/2; i++) {
            for (int j=0; j<w/2; j++) {
                int p1off = planesize[0];
                int p2off = planesize[0]+planesize[1];

                if (i*2 >= offset && i*2 < (offset+thick)) {
                    buffer[p1off + (i * (w/2) + j)] = color2[1];
                    buffer[p2off + (i * (w/2) + j)] = color2[2];
                } else {
                    buffer[p1off + (i * (w/2) + j)] = color1[1];
                    buffer[p2off + (i * (w/2) + j)] = color1[2];
                }
            }
        }
    }
};

class EncoderDecoderFile : public EncoderDecoder {
public:
    struct FileStat {
        std::string filename;
        uint32_t width;
        uint32_t height;
        uint32_t count;
    };
    EncoderDecoderFile(FileStat fs, RecoverType recover = IDR_RECOVER, uint32_t lr = 0)
    : EncoderDecoder(fs.width, fs.height, fs.count, recover, lr), filename(fs.filename) {

    }
private:
    void contentSetup() override {
        ifs.open (filename.c_str(), std::ifstream::in | std::ifstream::binary);
        assert (ifs.is_open());
        offset = 0;

        uint8_t* _mem = (uint8_t*) malloc(frameMemSize);
        mem = std::shared_ptr<uint8_t>(_mem, [](uint8_t* ptr){free(ptr);});
    }

    std::shared_ptr<uint8_t> getContent(uint32_t hint) override {
        ifs.read((char*)mem.get(), frameMemSize);

        return mem;
    }

    void contentTearDown() override {
        if (!ifs.is_open()) return;
        ifs.close();
    }

    std::string filename;
    std::ifstream ifs;
    uint64_t offset;
    std::shared_ptr<uint8_t> mem;
};

int main() {
    //EncoderDecoder ed1(320,240,120,NOT_RECOVER, 40);
    //ed1.start();

    //EncoderDecoder ed2(320,240,120,IDR_RECOVER, 40);
    //ed2.start();

    //EncoderDecoder ed3(320,240,120,LTR_RECOVER, 40);
    //ed3.start();

    //EncoderDecoderFile ed4({"bbb_352x288_420p_30fps_32frames.yuv",352,288,32}, IDR_RECOVER, 10);
    //ed4.start();

    EncoderDecoderFile ed5({"bbb_352x288_420p_30fps_32frames.yuv",352,288,32}, LTR_RECOVER, 20);
    ed5.start();
}
