#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <gl/gl.h>
#include "base.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")

#define EXR_MAGIC 0x01312f76 // Little-endian for 76 2f 31 01

#define SAVE_PATH "save.bmp"

global u32 g_Running;
global HWND g_Window;
global vec2 g_WindowSize;

global image g_Image;
global u32 g_Texture;

internal void
SaveBMP(image* Image, char* FilePath)
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

internal u32
CreateTexture(image Image)
{
    u32 Result = {0};
    
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &Result);
    glBindTexture(GL_TEXTURE_2D, Result);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, Image.Width, Image.Height, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, Image.Data);
    
    return Result;
}

internal void
Render()
{
    glViewport(0, 0, g_WindowSize.x, g_WindowSize.y);
    f32 p = 1.0f;
    glBegin(GL_TRIANGLES);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-p, -p);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(p, p);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(p, -p);
    
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-p, -p);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-p, p);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(p, p);
    glEnd();
}

internal void
Clear()
{
    glClearColor(1.0f, 0.0, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
internal void
InitOpenGL(HDC WindowDC)
{
    PIXELFORMATDESCRIPTOR PixelFormat = {0};
    PixelFormat.nVersion = 1;
    PixelFormat.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    PixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    PixelFormat.cColorBits = 24;
    PixelFormat.cAlphaBits = 8;
    s32 PixelFormatIndex = ChoosePixelFormat(WindowDC, &PixelFormat);
    SetPixelFormat(WindowDC, PixelFormatIndex, &PixelFormat);
    HGLRC Context = wglCreateContext(WindowDC);
    wglMakeCurrent(WindowDC, Context);
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

internal bmp_color
BMPColorFromColorF32(f32 R, f32 G, f32 B, f32 Exposure)
{
    bmp_color Result;
    
    // Exposure
    f32 Scale = powf(2.0f, Exposure);
    R *= Scale;
    G *= Scale;
    B *= Scale;
    
    // Tonemap (simple Reindhart)
    R = (R / (R + 1.0f));
    G = (G / (G + 1.0f));
    B = (B / (B + 1.0f));
    
    Clamp(0.0f, 1.0f, R);
    Clamp(0.0f, 1.0f, G);
    Clamp(0.0f, 1.0f, B);
    
    Result.r = (u8)(R * 255.0f);
    Result.g = (u8)(G * 255.0f);
    Result.b = (u8)(B * 255.0f);
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
ParseEXR(char* FilePath)
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
    bmp_color* ImageBuffer = (bmp_color*)malloc(sizeof(bmp_color) * Width * Height);
    bmp_color* CurrentImageBuffer = ImageBuffer;
    
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
            *CurrentImageBuffer = BMPColorFromColorF32(FloatFromHalf(R[j]), FloatFromHalf(G[j]), FloatFromHalf(B[j]), 1.0f);
            CurrentImageBuffer++;
        }
    }
    
    Result.Width = Width;
    Result.Height = Height;
    Result.Channels = 3;
    Result.Data = (u8*)ImageBuffer;
    
    return Result;
}

LRESULT
WndProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
    LRESULT Result = {0};
    
    switch(Message)
    {
        case WM_SIZE:
        {
            g_WindowSize = {(f32)(lParam & 0xFFFF), (f32)(lParam >> 16 & 0xFFFF)};
            break;
        }
        case WM_KEYDOWN:
        {
            char Key = (char) wParam;
            switch(Key)
            {
                case VK_ESCAPE:
                {
                    DestroyWindow(Window);
                    break;
                }
                case VK_SPACE:
                {
                    if(g_Image.Width > 0)
                    {
                        SaveBMP(&g_Image, "../data/save.bmp");
                    }
                    break;
                }
            }
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            g_Running = false;
            break;
        }
        default:
        {
            Result = DefWindowProc(Window, Message, wParam, lParam);
            break;
        }
    }
    return Result;
}
int main(int NumArgs, char** Args)
{
    setvbuf(stdout, 0, _IONBF, 0);
    
    if(NumArgs != 2)
    {
        printf("Usage: win_exrviewer.exe [exr_file_path]\n");
        return -1;
    }
    
    char* FilePath = Args[1];
    printf("EXR file to view: %s\n", FilePath);
    
    
    
    HMODULE hInstance = GetModuleHandle(0);
    WNDCLASS WndClass = {0};
    WndClass.hInstance = hInstance;
    WndClass.lpszClassName = "WndClass";
    WndClass.lpfnWndProc = WndProc;
    WndClass.hCursor     = LoadCursor(0, IDC_ARROW);
    RegisterClass(&WndClass);
    
    g_Window = CreateWindow(WndClass.lpszClassName,
                            "EXRViewer (Press space to save image to save.bmp",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
                            0, 0, 400, 400,
                            0, 0, hInstance, 0);
    HDC WindowDC = GetDC(g_Window);
    InitOpenGL(WindowDC);
    
    g_Image = ParseEXR(FilePath);
    g_Texture = CreateTexture(g_Image);
    
    g_Running = true;
    MSG Message = {0};
    while(g_Running)
    {
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&Message);
        }
        
        Clear();
        Render();
        SwapBuffers(WindowDC);
    }
    
    
    return 0;
}