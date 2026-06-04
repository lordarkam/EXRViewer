#include "exr_parser.h"

internal void
SaveBMP(image* Image, char* FilePath)
{
    if(Image->Type == IMAGE_DATA_BGR8)
    {
        
        FILE* File = fopen(FilePath, "wb");
        if(!File)
        {
            printf("Error creating file %s\n", FilePath);
            return;
        }
        
        bmp_file BmpHeader = {0};
        u32 DataSize = Image->Width * Image->Height * Image->Channels;
        u32 FileSize = offsetof(BmpHeader, Data) + DataSize;
        
        BmpHeader.ID[0]        = 'B';
        BmpHeader.ID[1]        = 'M';
        BmpHeader.FileSize     = FileSize;
        BmpHeader.DataOffset   = offsetof(BmpHeader, Data);
        BmpHeader.HeaderSize   = offsetof(BmpHeader, Data) - offsetof(BmpHeader, HeaderSize);
        BmpHeader.Width        = Image->Width;
        BmpHeader.Height       = -Image->Height;
        BmpHeader.Planes       = 1;
        BmpHeader.BitsPerPixel = Image->Channels * 8;
        BmpHeader.DataSize     = DataSize;
        
        fwrite(&BmpHeader, 1, offsetof(BmpHeader, Data), File);
        fwrite(Image->Data, 1, BmpHeader.DataSize, File);
        fclose(File);
    }
    else
    {
        printf("Image format is not compatible with BMP file format\n");
    }
}


internal str
ReadFullFile(char* FilePath)
{
    str Result = {0};
    
    FILE* File = fopen(FilePath, "rb");
    if(!File)
    {
        printf("Error opening file: %s\n", FilePath);
        return Result;
    }
    fseek(File, 0, SEEK_END);
    Result.Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    Result.String = (char*)malloc(Result.Size);
    fread(Result.String, 1, Result.Size, File);
    fclose(File);
    return Result;
}

internal str
ReadStr(u8** Current, char* Buffer, u32 BufferSize)
{
    str Result = {Buffer, 0};
    
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

inline internal color_f32
ApplyExposure(color_f32 Color, f32 Exposure)
{
    color_f32 Result;
    
    // Exposure
    f32 Scale = powf(2.0f, Exposure);
    Result.r = Color.r * Scale;
    Result.g = Color.g * Scale;
    Result.b = Color.b * Scale;
    
    return Result;
}

inline internal color_f32
ApplyTonemap(color_f32 Color)
{
    Color.r = (Color.r / (Color.r + 1.0f));
    Color.g = (Color.g / (Color.g + 1.0f));
    Color.b = (Color.b / (Color.b + 1.0f));
    
    Color.r = Clamp(0.0f, 1.0f, Color.r);
    Color.g = Clamp(0.0f, 1.0f, Color.g);
    Color.b = Clamp(0.0f, 1.0f, Color.b);
    
    return Color;
}

internal bmp_color
BMPColorFromColorF32(color_f32 Color, f32 Exposure)
{
    bmp_color Result;
    
    // Exposure
    color_f32 ExposuredColor = ApplyExposure(Color, Exposure);
    
    // Tonemap (simple Reindhart)
    color_f32 TonemappedColor = ApplyTonemap(ExposuredColor);
    
    Result.r = (u8)(TonemappedColor.r * 255.0f);
    Result.g = (u8)(TonemappedColor.g * 255.0f);
    Result.b = (u8)(TonemappedColor.b * 255.0f);
    
    return Result;
}

internal color_f32
ColorFromColorF32(color_f32 Color, f32 Exposure)
{
    color_f32 Result;
    
    // Exposure
    color_f32 ExposuredColor = ApplyExposure(Color, Exposure);
    
    // Tonemap (simple Reindhart)
    color_f32 TonemappedColor = ApplyTonemap(ExposuredColor);
    
    Result = TonemappedColor;
    
    return Result;
}

internal f32
FloatFromHalf(u16 HalfVal)
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
    
    // Direct bit-cast via union (common pattern in this style to avoid memcpy)
    union
    { 
        u32 U; 
        f32 F; 
    } Cast;
    
    Cast.U = FloatBits;
    
    return Cast.F;
}

internal image
ParseEXR(char* FilePath, image_data_type ImageType, f32 Exposure = 1.0f)
{
    image Result = {0};
    
    str FileData = ReadFullFile(FilePath);
    if(!FileData.Size)
    {
        printf("Error parsing .exr file: %s\n", FilePath);
        return Result;
    }
    
    u8* Current = (u8*)FileData.String;
    
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
        str AttrName = ReadStr(&Current, AttrNameBuffer, sizeof(AttrNameBuffer));
        if(!AttrName.Size)
        {
            // End of the header
            break;
        }
        
        str AttrType = ReadStr(&Current, AttrTypeBuffer, sizeof(AttrTypeBuffer));
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
        }
    }
    
    //Skip Line Offset Table
    // Uncompressed EXR has 1 offset entry per scanline row
    Current += sizeof(u64) * Height;
    
    // Buffer to contain the interleaved data
    u32 ImageElementSize = ImageType == IMAGE_DATA_BGR8 ? sizeof(bmp_color) : sizeof(color_f32);
    
    u8* ImageBuffer = (u8*)malloc(ImageElementSize * Width * Height);
    u8* CurrentImageBuffer = ImageBuffer;
    
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
                *((bmp_color*)CurrentImageBuffer) = BMPColorFromColorF32({FloatFromHalf(R[j]), FloatFromHalf(G[j]), FloatFromHalf(B[j])}, Exposure) ;
            }
            else
            {
                *((color_f32*)CurrentImageBuffer) = ColorFromColorF32({FloatFromHalf(R[j]), FloatFromHalf(G[j]), FloatFromHalf(B[j])}, Exposure) ;
            }
            
            CurrentImageBuffer += ImageElementSize;
        }
    }
    
    Result.Width    = Width;
    Result.Height   = Height;
    Result.Channels = 3;
    Result.Type     = ImageType;
    Result.Data     = (u8*)ImageBuffer;
    
    return Result;
}
