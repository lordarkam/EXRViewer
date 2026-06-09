/* date = June 4th 2026 1:22 pm */
//--------------------------------------------------------------------------------------------------------------------------
// Simple and slow exr file loader. It only handle half 16 uncompressed images 
//
// The user API is this simple function:
// exr_image EXRLoad(char* FileBuffer, exr_image_data_type ImageType,  char* ImageBuffer)
// 
// Usage: First, call EXRLoad() with a buffer containing the whole exr file data and null as the third argument
// This returns a placeholder exr_image with the basic image data to let the user allocate the image buffer.
// The user should allocate a buffer of size exr_image::DataSize
// That buffer should be passed as the third argument in a second invokation to EXRLoad (the first argumnet should be the same 
// file buffer than in the initial call)
// All the buffers (file and image) are managed by the user, 0 allocation inside the EXRLoad function;
//
//--------------------------------------------------------------------------------------------------------------------------


#ifndef EXRVIEWER_H
#define EXRVIEWER_H

#ifndef  GL_RGB32F 
#define GL_RGB32F 0x8815 // OpenGL format definition
#endif

#define EXR_MAGIC 0x01312f76 // Little-endian for 76 2f 31 01

#ifndef  Min
#define Min(x, y) ((x) <= (y)) ? x : y
#endif
#ifndef  Max
#define Max(x, y) ((x) >= (y)) ? x : y
#endif
#ifndef  Clamp
#define Clamp(min, max, value) Min(Max((min), (value)), (max))
#endif

#ifndef ReadU32
#define ReadU32(Current) (Current += sizeof(u32), *(u32*)(Current - sizeof(u32)))
#endif
#ifndef ReadS32
#define ReadS32(Current) (Current += sizeof(s32), *(s32*)(Current - sizeof(s32)))
#endif

struct exr_str
{
    char* String;
    u32   Size;
};

struct exr_color_f32
{
    f32 r,g,b;
};
struct exr_bmp_color
{
    u8 r,g,b;
};

enum exr_image_data_type
{
    IMAGE_DATA_BGR8,
    IMAGE_DATA_RGB32,
};

struct exr_image
{
    s32 Width;
    s32 Height;
    u32 Channels;
    exr_image_data_type Type;
    u32 DataSize;
    u8* Data;
};

struct exr_context
{
    u8* ImageDataPointer;
    exr_image ImageData;
};
#endif //EXRVIEWER_H

#ifdef EXR_LOADER_IMPLEMENTATION

global exr_context g_ExrContext;

internal exr_str
EXRReadStr(u8** Current, char* Buffer, u32 BufferSize)
{
    exr_str Result = {Buffer, 0};
    
    while(Result.Size < BufferSize)
    {
        char C = *((*Current)++);
        if(C == EOF || C == 0)
        {
            return Result;
        }
        
        Result.String[Result.Size++] = C;
    }
    
    Result.Size = 0;
    
    return Result;
}

inline internal exr_color_f32
EXRApplyExposure(exr_color_f32 Color, f32 Exposure)
{
    exr_color_f32 Result;
    
    // Exposure
    f32 Scale = powf(2.0f, Exposure);
    Result.r = Color.r * Scale;
    Result.g = Color.g * Scale;
    Result.b = Color.b * Scale;
    
    return Result;
}

inline internal exr_color_f32
EXRApplyTonemap(exr_color_f32 Color)
{
    Color.r = (Color.r / (Color.r + 1.0f));
    Color.g = (Color.g / (Color.g + 1.0f));
    Color.b = (Color.b / (Color.b + 1.0f));
    
    Color.r = Clamp(0.0f, 1.0f, Color.r);
    Color.g = Clamp(0.0f, 1.0f, Color.g);
    Color.b = Clamp(0.0f, 1.0f, Color.b);
    
    return Color;
}

internal exr_bmp_color
EXRBMPColorFromColorF32(exr_color_f32 Color, f32 Exposure)
{
    exr_bmp_color Result;
    
    // Exposure
    exr_color_f32 ExposuredColor = EXRApplyExposure(Color, Exposure);
    
    // Tonemap (simple Reindhart)
    exr_color_f32 TonemappedColor = EXRApplyTonemap(ExposuredColor);
    
    Result.r = (u8)(TonemappedColor.r * 255.0f);
    Result.g = (u8)(TonemappedColor.g * 255.0f);
    Result.b = (u8)(TonemappedColor.b * 255.0f);
    
    return Result;
}

internal exr_color_f32
EXRColorFromColorF32(exr_color_f32 Color, f32 Exposure)
{
    exr_color_f32 Result;
    
    // Exposure
    exr_color_f32 ExposuredColor = EXRApplyExposure(Color, Exposure);
    
    // Tonemap (simple Reindhart)
    exr_color_f32 TonemappedColor = EXRApplyTonemap(ExposuredColor);
    
    Result = TonemappedColor;
    
    return Result;
}

internal f32
EXRFloatFromHalf(u16 HalfVal)
{
    u32 Sign     = (u32)(HalfVal & 0x8000) << 16;
    u32 Exponent = (u32)(HalfVal & 0x7C00) >> 10;
    u32 Mantissa = (u32)(HalfVal & 0x03FF);
    
    u32 FloatBits = 0;
    
    if(Exponent == 0)
    {
        if(Mantissa != 0)
        {
            // Denormalized number
            Exponent = 127 - 14;
            while(!(Mantissa & 0x0400))
            {
                Mantissa <<= 1;
                Exponent--;
            }
            Mantissa &= 0x03FF;
            FloatBits = Sign | (Exponent << 23) | (Mantissa << 13);
        }
        else
        {
            // Zero
            FloatBits = Sign;
        }
    }
    else if(Exponent == 31)
    {
        // Infinity or NaN
        FloatBits = Sign | (0xFF << 23) | (Mantissa << 13);
    }
    else
    {
        // Normalized number
        FloatBits = Sign | ((Exponent + (127 - 15)) << 23) | (Mantissa << 13);
    }
    
    return *(f32*)(&FloatBits);
}

internal exr_image
EXRLoad(char* FileBuffer, exr_image_data_type ImageType, char* ImageBuffer, f32 Exposure = 1.0f)
{
    exr_image Result = {0};
    
    if(!ImageBuffer)
    {
        u8* Current = (u8*)FileBuffer;
        
        u32 Magic = ReadU32(Current);
        if(Magic != EXR_MAGIC)
        {
            printf("Error: File is not a valide .EXR file.\n");
            return Result;
        }
        
        u32 Version = ReadU32(Current);
        if(Version != 2)
        {
            printf("Error: EXR file version 2 expected\n");
            return Result;
        }
        printf("EXP Version: %d\n", Version);
        
        // Image metadata variables to extract from header
        s32 xMin = 0, yMin = 0, xMax = 0, yMax = 0;
        s32 Width = 0, Height = 0;
        char CompressionType = -1;
        
        char AttrNameBuffer[256];
        char AttrTypeBuffer[256];
        s32  AttrSize = {0};
        
        while(true)
        {
            exr_str AttrName = EXRReadStr(&Current, AttrNameBuffer, sizeof(AttrNameBuffer));
            if(!AttrName.Size)
            {
                // End of the header
                break;
            }
            
            exr_str AttrType = EXRReadStr(&Current, AttrTypeBuffer, sizeof(AttrTypeBuffer));
            AttrSize = ReadU32(Current);
            
            // TODO(jmendoza): Check the channels attribute to ensure it's an rgb16 fomrat (the only supported for this viewer)
            if(!strncmp(AttrName.String, "compression", AttrName.Size))
            {
                CompressionType = *((char*)Current++);
            }
            else if(!strncmp(AttrName.String, "dataWindow", AttrName.Size))
            { 
                xMin = ReadS32(Current);
                yMin = ReadS32(Current);
                xMax = ReadS32(Current);
                yMax = ReadS32(Current);
                Width = xMax - xMin + 1;
                Height = yMax - yMin + 1;
            }
            else
            {
                Current+= AttrSize;
            }
        }
        
        if(Width != 0 && Height != 0)
        {
            printf("Width %d Height %d Compression %d\n", Width, Height, CompressionType);
            if(CompressionType != 0)
            {
                printf("Error: this parser only works for non compressed .exr files\n");
                return Result;
            }
        }
        
        u32 ImageElementSize = ImageType == IMAGE_DATA_BGR8 ? sizeof(exr_bmp_color) : sizeof(exr_color_f32);
        
        Result.Width    = Width;
        Result.Height   = Height;
        Result.Channels = 3;
        Result.Type     = ImageType;
        Result.DataSize = ImageElementSize * Width * Height;
        
        g_ExrContext.ImageDataPointer = Current;
        g_ExrContext.ImageData = Result;
        
        return Result;
    }
    else
    {
        u8* Current = g_ExrContext.ImageDataPointer;
        s32 Width = g_ExrContext.ImageData.Width;
        s32 Height = g_ExrContext.ImageData.Height;
        
        //Skip Line Offset Table
        // Uncompressed EXR has 1 offset entry per scanline row
        Current += sizeof(u64) * Height;
        
        // Buffer to contain the interleaved data
        u32 ImageElementSize = ImageType == IMAGE_DATA_BGR8 ? sizeof(exr_bmp_color) : sizeof(exr_color_f32);
        
        u8* CurrentImageBuffer = (u8*)ImageBuffer;
        
        for(u32 i = 0; i < Height; i++)
        {
            s32 RowIndex = ReadS32(Current);
            s32 PixelDataSize = ReadU32(Current);
            
            // Temporary row storage for swapping planar layouts ( Blue, Green, Red, alphabetical order)
            u16* B = (u16*)Current;
            Current += sizeof(u16) * Width;
            
            u16* G = (u16*)Current;
            Current += sizeof(u16) * Width;
            
            u16* R = (u16*)Current;
            Current += sizeof(u16) * Width;
            
            for(u32 j = 0; j < Width; j++)
            {
                if(ImageType == IMAGE_DATA_BGR8)
                {
                    *((exr_bmp_color*)CurrentImageBuffer) = EXRBMPColorFromColorF32({EXRFloatFromHalf(R[j]), EXRFloatFromHalf(G[j]), EXRFloatFromHalf(B[j])}, Exposure) ;
                }
                else
                {
                    *((exr_color_f32*)CurrentImageBuffer) = EXRColorFromColorF32({EXRFloatFromHalf(R[j]), EXRFloatFromHalf(G[j]), EXRFloatFromHalf(B[j])}, Exposure) ;
                }
                CurrentImageBuffer += ImageElementSize;
            }
        }
        
        Result = g_ExrContext.ImageData;
        Result.Data     = (u8*)ImageBuffer;
        
        return Result;
    }
}

#endif