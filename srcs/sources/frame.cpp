#include "common/frame.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

template <typename T>
void SafeDeleteArray(void *&p)
{
    if (p)
    {
        delete[] reinterpret_cast<T *>(p);
        p = nullptr;
    }
}

} // namespace

Frame::Frame(ImageInfo &img_info)
{
    info = img_info;
    std::memset(&data, 0, sizeof(data));
    FrameDateMalloc(info.width, info.height);
}

Frame::~Frame()
{
    SafeDeleteArray<int32_t>(data.raw_s32_i);
    SafeDeleteArray<int32_t>(data.raw_s32_o);
    SafeDeleteArray<int32_t>(data.bgr_s32_i);
    SafeDeleteArray<int32_t>(data.bgr_s32_o);
    SafeDeleteArray<uint8_t>(data.bgr_u8_o);

    // Other buffers are unused in this minimal pipeline example.
    data.raw_u8_i = nullptr;
    data.raw_u16_i = nullptr;
    data.raw_u16_o = nullptr;
    data.yuv_f32_i = {};
    data.yuv_f32_o = {};
    data.yuv_u8_i = {};
    data.yuv_u8_o = {};
}

int Frame::FrameDateMalloc(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return -1;
    }

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);

    data.raw_s32_i = new int32_t[pixel_count];
    data.raw_s32_o = new int32_t[pixel_count];
    data.bgr_s32_i = new int32_t[pixel_count * 3];
    data.bgr_s32_o = new int32_t[pixel_count * 3];
    data.bgr_u8_o = new uint8_t[pixel_count * 3];

    std::memset(data.raw_s32_i, 0, pixel_count * sizeof(int32_t));
    std::memset(data.raw_s32_o, 0, pixel_count * sizeof(int32_t));
    std::memset(data.bgr_s32_i, 0, pixel_count * 3 * sizeof(int32_t));
    std::memset(data.bgr_s32_o, 0, pixel_count * 3 * sizeof(int32_t));
    std::memset(data.bgr_u8_o, 0, pixel_count * 3 * sizeof(uint8_t));

    return 0;
}

int Frame::ReadFileToFrame(std::string file, int size)
{
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs)
    {
        return -1;
    }

    ifs.seekg(0, std::ios::end);
    const std::streamoff file_size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (size > 0 && file_size != static_cast<std::streamoff>(size))
    {
        return -1;
    }

    const int bytes_to_read = (size > 0) ? size : static_cast<int>(file_size);
    if (bytes_to_read <= 0)
    {
        return -1;
    }

    std::vector<uint8_t> buf(static_cast<size_t>(bytes_to_read));
    ifs.read(reinterpret_cast<char *>(buf.data()), bytes_to_read);
    if (ifs.gcount() != bytes_to_read)
    {
        return -1;
    }

    return RawMemToFrame(buf.data(), bytes_to_read);
}

int Frame::RawMemToFrame(void *src, int len)
{
    if (!src || len <= 0)
    {
        return -1;
    }

    const size_t pixel_count = static_cast<size_t>(info.width) * static_cast<size_t>(info.height);

    const size_t expected_bytes = info.mipi_packed
                                      ? static_cast<size_t>((static_cast<uint64_t>(pixel_count) * static_cast<uint64_t>(info.bpp) + 7u) / 8u)
                                      : pixel_count * static_cast<size_t>((info.bpp + 7) / 8);
    if (static_cast<size_t>(len) < expected_bytes)
    {
        return -1;
    }

    int32_t *dst = reinterpret_cast<int32_t *>(data.raw_s32_i);

    if (info.mipi_packed)
    {
        // Minimal support needed for this repo demo: MIPI packed RAW10.
        // Packing: 5 bytes store 4 pixels (10-bit):
        // p0 = b0 | ((b4 & 0x03) << 8)
        // p1 = b1 | (((b4 >> 2) & 0x03) << 8)
        // p2 = b2 | (((b4 >> 4) & 0x03) << 8)
        // p3 = b3 | (((b4 >> 6) & 0x03) << 8)
        if (info.dt != RawDataTypes::RAW10 && info.bpp != 10)
        {
            return -1;
        }

        const uint8_t *b = reinterpret_cast<const uint8_t *>(src);
        const size_t groups = pixel_count / 4;
        size_t out = 0;
        size_t in = 0;
        for (size_t g = 0; g < groups; ++g)
        {
            const uint8_t b0 = b[in + 0];
            const uint8_t b1 = b[in + 1];
            const uint8_t b2 = b[in + 2];
            const uint8_t b3 = b[in + 3];
            const uint8_t b4 = b[in + 4];

            dst[out + 0] = static_cast<int32_t>(static_cast<uint16_t>(b0) | (static_cast<uint16_t>(b4 & 0x03) << 8));
            dst[out + 1] = static_cast<int32_t>(static_cast<uint16_t>(b1) | (static_cast<uint16_t>((b4 >> 2) & 0x03) << 8));
            dst[out + 2] = static_cast<int32_t>(static_cast<uint16_t>(b2) | (static_cast<uint16_t>((b4 >> 4) & 0x03) << 8));
            dst[out + 3] = static_cast<int32_t>(static_cast<uint16_t>(b3) | (static_cast<uint16_t>((b4 >> 6) & 0x03) << 8));

            in += 5;
            out += 4;
        }

        // If pixel_count is not divisible by 4, we don't support partial tail packing here.
        if (out != pixel_count)
        {
            return -1;
        }
    }
    else
    {
        const int bytes_per_pixel = (info.bpp + 7) / 8;
        if (bytes_per_pixel == 1)
        {
            const uint8_t *u8 = reinterpret_cast<const uint8_t *>(src);
            for (size_t i = 0; i < pixel_count; ++i)
            {
                dst[i] = static_cast<int32_t>(u8[i]);
            }
        }
        else
        {
            // Assume little-endian unpacked RAW stored as uint16 per pixel.
            const uint16_t *u16 = reinterpret_cast<const uint16_t *>(src);
            for (size_t i = 0; i < pixel_count; ++i)
            {
                dst[i] = static_cast<int32_t>(u16[i]);
            }
        }
    }

    return 0;
}
