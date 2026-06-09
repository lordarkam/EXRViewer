#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <gl/gl.h>
#include "base.h"

#define EXR_LOADER_IMPLEMENTATION
#include "exr_parser.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")

#define SAVE_PATH "save.bmp"

global u32 g_Running;
global HWND g_Window;
global vec2 g_WindowSize;

global exr_image g_Image;
global u32 g_Texture;


internal void
SaveBMP(exr_image* Image, char* FilePath)
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


internal u32
CreateTexture(exr_image Image)
{
    u32 Result = {0};
    
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &Result);
    glBindTexture(GL_TEXTURE_2D, Result);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    u32 InternalFormat = Image.Type == IMAGE_DATA_BGR8 ? GL_RGB8 : GL_RGB32F;
    u32 Format = Image.Type == IMAGE_DATA_BGR8 ? GL_BGR_EXT : GL_RGB;
    u32 DataType = Image.Type == IMAGE_DATA_BGR8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
    glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Image.Width, Image.Height, 0, Format, DataType, Image.Data);
    
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

internal exr_image
LoadEXR(char* FilePath)
{
    exr_image Result = {0};
    
    str FileData = ReadFullFile(FilePath);
    if(FileData.Size == 0)
    {
        printf("Error: file .exr is not present %s\n", FilePath);
        return Result;
    }
    Result = EXRLoad(FileData.String, IMAGE_DATA_BGR8, 0);
    char* ImageBuffer = (char*)malloc(Result.DataSize);
    Result = EXRLoad(FileData.String, IMAGE_DATA_BGR8, ImageBuffer, 5);
    free(FileData.String);
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
    
    g_Image = LoadEXR(FilePath);
    g_Texture = CreateTexture(g_Image);
    free(g_Image.Data);
    
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