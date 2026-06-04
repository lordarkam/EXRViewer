#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <gl/gl.h>
#include "base.h"
#include "exr_parser.cpp"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")


#define SAVE_PATH "save.bmp"

global u32 g_Running;
global HWND g_Window;
global vec2 g_WindowSize;

global image g_Image;
global u32 g_Texture;

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
    
    g_Image = ParseEXR(FilePath, IMAGE_DATA_BGR8, 5);
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