/* date = June 4th 2026 1:22 pm */

#ifndef EXRVIEWER_H
#define EXRVIEWER_H

#define GL_RGB32F 0x8815

#define EXR_MAGIC 0x01312f76 // Little-endian for 76 2f 31 01

#define Min(x, y) ((x) <= (y)) ? x : y
#define Max(x, y) ((x) >= (y)) ? x : y
#define Clamp(min, max, value) Min(Max((min), (value)), (max))


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

struct color_f32
{
    f32 r,g,b;
};

enum image_data_type
{
    IMAGE_DATA_BGR8,
    IMAGE_DATA_RGB32,
};

struct image
{
    s32 Width;
    s32 Height;
    u32 Channels;
    image_data_type Type;
    u8* Data;
};

struct vec2
{
    f32 x,y;
};

#define ReadU32(Current) (Current += sizeof(u32), *(u32*)(Current - sizeof(u32)))
#define ReadS32(Current) (Current += sizeof(s32), *(s32*)(Current - sizeof(s32)))


#endif //EXRVIEWER_H
