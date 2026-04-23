#include "modules/modules.h"

#include <algorithm>
#include <iostream>

#define MOD_NAME "dpc"

int Dpc(Frame *frame, const IspPrms *isp_prm) {
    if ((frame == nullptr) || (isp_prm == nullptr)) {
        std::cerr << "input prms is null" << std::endl;
        return -1;
    }

    int32_t *raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);
    int32_t *raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);

    int thres = isp_prm->dpc_prms.thres;
    auto mode = isp_prm->dpc_prms.mode;

    for (int ih = 0; ih < frame->info.height; ih++) {
        for (int iw = 0; iw < frame->info.width; iw++) {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);

            // 边界像素不处理
            if ((iw < 2) || (iw >= (frame->info.width - 2)) || (ih < 2) || (ih >= (frame->info.height - 2))) {
                raw32_out[pixel_idx] = raw32_in[pixel_idx];
                continue;
            }

            /*
             p1 x  p2 x  p3
             x  x  x  x  x
             p4 x  p0 x  p5
             x  x  x  x  x
             p6 x  p7 x  p8
            */
            int p0 = raw32_in[GET_PIXEL_INDEX(iw, ih, frame->info.width)];
            int p1 = raw32_in[GET_PIXEL_INDEX((iw - 2), (ih - 2), frame->info.width)];
            int p2 = raw32_in[GET_PIXEL_INDEX(iw, (ih - 2), frame->info.width)];
            int p3 = raw32_in[GET_PIXEL_INDEX((iw + 2), (ih - 2), frame->info.width)];
            int p4 = raw32_in[GET_PIXEL_INDEX((iw - 2), ih, frame->info.width)];
            int p5 = raw32_in[GET_PIXEL_INDEX((iw + 2), ih, frame->info.width)];
            int p6 = raw32_in[GET_PIXEL_INDEX((iw - 2), (ih + 2), frame->info.width)];
            int p7 = raw32_in[GET_PIXEL_INDEX(iw, (ih + 2), frame->info.width)];
            int p8 = raw32_in[GET_PIXEL_INDEX((iw + 2), (ih + 2), frame->info.width)];

            // 如果中心像素与领域像素差值都大于阈值，则认为是坏点
            if ((abs(p1 - p0) > thres) && (abs(p2 - p0) > thres) && (abs(p3 - p0) > thres) &&
                (abs(p4 - p0) > thres) && (abs(p5 - p0) > thres) && (abs(p6 - p0) > thres) &&
                (abs(p7 - p0) > thres) && (abs(p8 - p0) > thres)) {
                // 均值模式：坏点替换为上下左右4个像素的平均值
                if (mode == DpcMode::MEAN) {
                    raw32_out[pixel_idx] = (p2 + p4 + p5 + p7) >> 2;
                }
                // 梯度模式：坏点替换为4个像素中与中心像素差值最小的那个
                else {
                    int dv = abs(p7 - p0 - p0 - p2);
                    int dh = abs(p5 - p0 - p0 - p4);
                    int dr = abs(p6 - p0 - p0 - p3);
                    int dl = abs(p8 - p0 - p0 - p1);

                    int min_diff = std::min({dv, dh, dr, dl});
                    if (min_diff == dv) {
                        raw32_out[pixel_idx] = (p2 + p7 + 1) >> 1;
                    }
                    else if (min_diff == dh) {
                        raw32_out[pixel_idx] = (p4 + p5 + 1) >> 1;
                    }
                    else if (min_diff == dr) {
                        raw32_out[pixel_idx] = (p3 + p6 + 1) >> 1;
                    }
                    else {
                        raw32_out[pixel_idx] = (p1 + p8 + 1) >> 1;
                    }
                }
            }
            else {
                raw32_out[pixel_idx] = raw32_in[pixel_idx];
            }
        }
    }

    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o);
    return 0;
}
