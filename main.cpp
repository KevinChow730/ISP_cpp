#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "modules/modules.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <wincodec.h>
#endif

// These functions are implemented in srcs/sources/*.cpp.
int Blc(Frame *frame, const IspPrms *isp_prm);
int Lsc(Frame *frame, const IspPrms *isp_prm);
int Dpc(Frame *frame, const IspPrms *isp_prm); // BPC/DPC
int Demoasic(Frame *frame, const IspPrms *isp_prm);

#ifdef _WIN32
static bool SaveBgr24ToJpegWic(const std::string &out_path, const uint8_t *bgr24, int width, int height)
{
    if (!bgr24 || width <= 0 || height <= 0) {
        return false;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool did_init_com = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    IWICImagingFactory *factory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        if (did_init_com) {
            CoUninitialize();
        }
        return false;
    }

    IWICStream *stream = nullptr;
    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        std::wstring wpath(out_path.begin(), out_path.end());
        hr = stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE);
    }

    IWICBitmapEncoder *encoder = nullptr;
    if (SUCCEEDED(hr)) {
        hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    }

    IWICBitmapFrameEncode *frame = nullptr;
    IPropertyBag2 *props = nullptr;
    if (SUCCEEDED(hr)) {
        hr = encoder->CreateNewFrame(&frame, &props);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Initialize(props);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    }

    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) {
        hr = frame->SetPixelFormat(&pixel_format);
    }

    const UINT stride = static_cast<UINT>(width * 3);
    const UINT image_bytes = stride * static_cast<UINT>(height);
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(static_cast<UINT>(height), stride, image_bytes, const_cast<BYTE *>(bgr24));
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Commit();
    }

    if (props)
        props->Release();
    if (frame)
        frame->Release();
    if (encoder)
        encoder->Release();
    if (stream)
        stream->Release();
    factory->Release();

    if (did_init_com) {
        CoUninitialize();
    }

    return SUCCEEDED(hr);
}
#endif

static std::vector<uint8_t> ConvertBgrS32ToBgrU8(const int32_t *bgr_s32, int width, int height, int max_val) {
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> out(pixel_count * 3);
    if (!bgr_s32 || max_val <= 0) {
        return out;
    }

    for (size_t i = 0; i < pixel_count * 3; ++i) {
        int32_t v = bgr_s32[i];
        if (v < 0)
            v = 0;
        if (v > max_val)
            v = max_val;
        const int32_t v8 = static_cast<int32_t>((static_cast<int64_t>(v) * 255 + (max_val / 2)) / max_val);
        out[i] = static_cast<uint8_t>(v8);
    }

    return out;
}

int main() {
    // Minimal example parameters (match cfgs/isp_config_dsc.json by default).
    IspPrms prms{};
    prms.raw_file = "./data/1920x1080_Raw10.raw";
    prms.out_file_path = "./";

    prms.sensor_name = "example";
    prms.info.cfa = CfaTypes::RGGB;
    prms.info.dt = RawDataTypes::RAW10;
    prms.info.bpp = 10;
    prms.info.max_val = (1 << 10) - 1;
    prms.info.width = 1920;
    prms.info.height = 1080;
    prms.info.mipi_packed = true;
    prms.info.domain = ColorDomains::RAW;
    prms.blc = 600;

    Frame frame(prms.info);
    const int64_t pixel_count = static_cast<int64_t>(prms.info.width) * static_cast<int64_t>(prms.info.height);
    const int expected_bytes = static_cast<int>((pixel_count * prms.info.bpp + 7) / 8);
    if (frame.ReadFileToFrame(prms.raw_file, expected_bytes) != 0) {
        // Fallback if user renamed the file.
        prms.raw_file = "./data/1920x1080_Raw10.RAW";
        if (frame.ReadFileToFrame(prms.raw_file, expected_bytes) != 0) {
            std::cerr << "Failed to read raw file: ./data/1920x1080_Raw10.raw (or ./data/1920x1080_Raw10.RAW)\n";
            std::cerr << "Expected bytes: " << expected_bytes << " (width=" << prms.info.width << ", height=" << prms.info.height
                      << ", bpp=" << prms.info.bpp << ", mipi_packed=" << (prms.info.mipi_packed ? "true" : "false") << ")\n";
            return 1;
        }
    }

    if (Blc(&frame, &prms) != 0)
        return 2;
    if (Lsc(&frame, &prms) != 0)
        return 3;
    if (Dpc(&frame, &prms) != 0)
        return 4;
    if (Demoasic(&frame, &prms) != 0)
        return 5;

    // Demoasic outputs packed BGR int32 into bgr_s32_i (after SwapMem).
    auto bgr8 = ConvertBgrS32ToBgrU8(reinterpret_cast<int32_t *>(frame.data.bgr_s32_i), prms.info.width, prms.info.height, prms.info.max_val);

    const std::string out_jpg = prms.out_file_path + std::string("output.jpg");
#ifdef _WIN32
    if (!SaveBgr24ToJpegWic(out_jpg, bgr8.data(), prms.info.width, prms.info.height)) {
        std::cerr << "Failed to write JPEG: " << out_jpg << "\n";
        return 6;
    }
#else
    std::cerr << "JPEG writer is only implemented for Windows in this example.\n";
    return 6;
#endif

    std::cout << "Wrote: " << out_jpg << "\n";
    return 0;
}