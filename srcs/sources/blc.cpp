#include <iostream>
#include "modules/modules.h"

#define MOD_NAME "blc"

int Blc(Frame* frame, const IspPrms* isp_prm) {
    if ((frame == nullptr) || (isp_prm == nullptr))
    {
        std::cerr << "input prms is null" << std::endl;
        return -1;
    }

    int32_t* raw32_in = reinterpret_cast<int32_t*>(frame->data.raw_s32_i);  // 输入图像数据指针，强制转换为int32_t类型
    int32_t* raw32_out = reinterpret_cast<int32_t*>(frame->data.raw_s32_o); // 输出图像数据指针，强制转换为int32_t类型

    for (int ih = 0; ih < frame->info.height; ih++) {
        for (int iw = 0; iw < frame->info.width; iw++) {
            int pixel_idx = GET_PIXEL_INDEX(iw, ih, frame->info.width); // 计算像素索引偏移
            raw32_out[pixel_idx] = raw32_in[pixel_idx] - isp_prm->blc; // 黑电平校正：输入像素值减去黑电平值
            ClipMinMax<int32_t>(raw32_out[pixel_idx], (int32_t)isp_prm->info.max_val, 0); // 将校正后的像素值裁剪在0和max_val之间
        }
    }

    SwapMem<void>(frame->data.raw_s32_i, frame->data.raw_s32_o); // 交换输入和输出图像数据指针，使输出成为新的输入
    
    return 0;
}
