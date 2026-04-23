#include "modules/modules.h"

#include <iostream>

#define MOD_NAME "lsc"

/*
 * gain0         gain1
 *
 *      . x_coff,y_coff
 *
 * gain2         gain3
 *
 * x_coff 0~1
 * y_coff 0~1
 */
static inline float BilinearInterpplation(float gain0,
                                          float gain1,
                                          float gain2,
                                          float gain3,
                                          float x_coff,
                                          float y_coff) {
    if (x_coff > 1 || y_coff > 1) {
        return 1;
    }

    float gain01 = (gain1 - gain0) * x_coff + gain0;
    float gain23 = (gain3 - gain2) * x_coff + gain2;

    return (gain23 - gain01) * y_coff + gain01;
}

int Lsc(Frame *frame, const IspPrms *isp_prm) {
    if ((frame == nullptr) || (isp_prm == nullptr)) {
        std::cerr << "input prms is null" << std::endl;
        return -1;
    }

    int32_t *raw32_in = reinterpret_cast<int32_t *>(frame->data.raw_s32_i);
    int32_t *raw32_out = reinterpret_cast<int32_t *>(frame->data.raw_s32_o);

    const LscPrms *lsc_prm = &(isp_prm->lsc_prms);

    const int mesh_box_width = static_cast<int>(ceil((float)frame->info.width / kLscMeshBoxHNums));
    const int mesh_box_height = static_cast<int>(ceil((float)frame->info.height / kLscMeshBoxVNums));

    for (int ih = 0; ih < frame->info.height; ih++) {
        for (int iw = 0; iw < frame->info.width; iw++) {
            const int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width);
            const int box_idx_w = iw / mesh_box_width;
            const int box_idx_h = ih / mesh_box_height;

            const float x_coff = (float)(iw % mesh_box_width) / mesh_box_width;
            const float y_coff = (float)(ih % mesh_box_height) / mesh_box_height;

            const int cfa_id = static_cast<int>(frame->info.cfa);
            float gain = 1;

            switch (kPixelCfaLut[cfa_id][iw % 2][ih % 2]) {
            case PixelCfaTypes::R:
                gain = BilinearInterpplation(lsc_prm->mesh_r[box_idx_h][box_idx_w], lsc_prm->mesh_r[box_idx_h][box_idx_w + 1],
                                             lsc_prm->mesh_r[box_idx_h + 1][box_idx_w], lsc_prm->mesh_r[box_idx_h + 1][box_idx_w + 1],
                                             x_coff, y_coff);
                break;
            case PixelCfaTypes::GR:
                gain = BilinearInterpplation(lsc_prm->mesh_gr[box_idx_h][box_idx_w], lsc_prm->mesh_gr[box_idx_h][box_idx_w + 1],
                                             lsc_prm->mesh_gr[box_idx_h + 1][box_idx_w], lsc_prm->mesh_gr[box_idx_h + 1][box_idx_w + 1],
                                             x_coff, y_coff);
                break;
            case PixelCfaTypes::GB:
                gain = BilinearInterpplation(lsc_prm->mesh_gb[box_idx_h][box_idx_w], lsc_prm->mesh_gb[box_idx_h][box_idx_w + 1],
                                             lsc_prm->mesh_gb[box_idx_h + 1][box_idx_w], lsc_prm->mesh_gb[box_idx_h + 1][box_idx_w + 1],
                                             x_coff, y_coff);
                break;
            case PixelCfaTypes::B:
                gain = BilinearInterpplation(lsc_prm->mesh_b[box_idx_h][box_idx_w], lsc_prm->mesh_b[box_idx_h][box_idx_w + 1],
                                             lsc_prm->mesh_b[box_idx_h + 1][box_idx_w], lsc_prm->mesh_b[box_idx_h + 1][box_idx_w + 1],
                                             x_coff, y_coff);
                break;
            default:
                break;
            }

            raw32_out[pixel_idx] = static_cast<int>(raw32_in[pixel_idx] * gain);
            ClipMinMax<int32_t>(raw32_out[pixel_idx], (int32_t)isp_prm->info.max_val, 0);
        }
    }

    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o);
    return 0;
}
