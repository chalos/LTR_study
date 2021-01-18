#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace codec {

    enum RecoverType : uint32_t {
        NOT_RECOVER = 0,
        IDR_RECOVER,
        LTR_RECOVER,
    };

    struct Plane {
        int32_t width;
        int32_t height;
        int32_t stride;
        uint8_t* data;
    };
    struct YUVFrame {
        Plane y;
        Plane u;
        Plane v;
        uint64_t ts;
    };
    enum FrameType {
        INVALID = 0,
        IDRFrame,
        IFrame,
        PFrame,
    };
    template <typename L, typename R>
    struct Mapper {
        Mapper(std::initializer_list<std::pair<L, R>> list) : map(list) {}
        bool loopup(const L& key, R* result=nullptr) const {
            for (auto item : map) {
                if (item.first == key) {
                    if(result) *result = item.second;
                    return true;
                }
            }
            return false;
        }
        bool rloopup(const R& key, L* result=nullptr) const {
            for (auto item : map) {
                if (item.second == key) {
                    if(result) *result = item.first;
                    return true;
                }
            }
            return false;
        }
    private:
        std::vector<std::pair<L, R>> map;
    };
};
