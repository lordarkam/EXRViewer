/* date = June 3rd 2026 5:49 pm */

#ifndef BASE_H
#define BASE_H

#include <stdint.h>

#define local static
#define internal static
#define global static

#define Min(x, y) ((x) <= (y)) ? x : y
#define Max(x, y) ((x) >= (y)) ? x : y
#define Clamp(min, max, value) Min(Max((min), (value)), (max))

typedef int8_t s8;
typedef uint8_t u8;
typedef uint8_t b8;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint16_t b16;
typedef int32_t s32;
typedef uint32_t u32;
typedef uint32_t b32;
typedef int64_t s64;
typedef uint64_t u64;
typedef uint64_t b64;
typedef float f32;
typedef double f64;

struct str
{
    char* String;
    u32   Size;
};

struct color_f32
{
    f32 r, g, b;
};

#pragma pack(push, 1)
struct bmp_file
{
    char ID[2];
    u32  FileSize;
    u32  Unused1;
    u32  DataOffset;
    u32  HeaderSize;
    s32  Width;
    s32  Heigth;
    u16  Planes;
    u16  BitsPerPixel;
    u32  Compression;
    u32  DataSize;
    u32  Unused[4];
    u8*  Data;
};

struct bmp_color
{
    u8 r, g, b;
};
#pragma pack(pop)

struct image
{
    s32 Width;
    s32 Height;
    u32 Channels;
    u8* Data;
};

struct vec2
{
    f32 x,y;
};

#define ReadU32(Current) (Current += sizeof(u32), *(u32*)(Current - sizeof(u32)))
#define ReadS32(Current) (Current += sizeof(s32), *(s32*)(Current - sizeof(s32)))

#endif //BASE_H
