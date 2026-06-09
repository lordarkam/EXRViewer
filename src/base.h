/* date = June 3rd 2026 5:49 pm */

#ifndef BASE_H
#define BASE_H

#include <stdint.h>

#define local static
#define internal static
#define global static

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

struct vec2
{
    f32 x,y;
};

struct str
{
    char* String;
    u32   Size;
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
    s32  Height;
    u16  Planes;
    u16  BitsPerPixel;
    u32  Compression;
    u32  DataSize;
    u32  Unused[4];
    u8*  Data;
};

struct bmp_color
{
    u8 b,g,r;
};
#pragma pack(pop)

#endif //BASE_H
