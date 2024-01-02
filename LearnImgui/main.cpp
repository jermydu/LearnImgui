#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <GL/GL.h>

//Windows 窗口数据
struct WGL_WindowData { HDC hDC; };
static HGLRC              g_hRC;          //opengl 句柄
static WGL_WindowData     g_MainWindow;     //窗口句柄
static int              g_Width;
static int              g_Height;

//初始化opengl
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(data->hDC);
    return true;
}

//释放opengl
void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

static void Hook_Renderer_CreateWindow(ImGuiViewport* viewport)
{
    assert(viewport->RendererUserData == nullptr);

    WGL_WindowData* data = IM_NEW(WGL_WindowData);
    CreateDeviceWGL((HWND)viewport->PlatformHandle, data);
    viewport->RendererUserData = data;
}

static void Hook_Renderer_DestroyWindow(ImGuiViewport* viewport)
{
    if (viewport->RendererUserData != nullptr)
    {
        WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData;
        CleanupDeviceWGL((HWND)viewport->PlatformHandle, data);
        IM_DELETE(data);
        viewport->RendererUserData = nullptr;
    }
}

static void Hook_Platform_RenderWindow(ImGuiViewport* viewport, void*)
{
    // Activate the platform window DC in the OpenGL rendering context
    if (WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData)
        wglMakeCurrent(data->hDC, g_hRC);
}

static void Hook_Renderer_SwapBuffers(ImGuiViewport* viewport, void*)
{
    if (WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData)
        ::SwapBuffers(data->hDC);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//窗口处理函数（自定义、处理消息）
LRESULT APIENTRY WndProc(HWND hWnd, UINT msgID, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msgID, wParam, lParam))
        return true;

    //处理消息
    switch (msgID)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) //禁用ALT应用程序菜单
            return 0;
        break;
    case WM_CREATE:
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    //给各种消息默认处理
    return DefWindowProc(hWnd, msgID, wParam, lParam);
}

int main(int argc, char* argv[])
{

    //注册窗口类
    WNDCLASS wc = { 0 };
    wc.style = CS_OWNDC;//窗口类的风格
    wc.lpfnWndProc = WndProc;//窗口处理函数
    wc.cbClsExtra = 0;//窗口类的附加数据buff的大小			
    wc.cbWndExtra = 0;//窗口的附加数据buff的大小
    wc.hInstance = GetModuleHandle(nullptr);//当前模块实例句柄
    wc.hIcon = nullptr;//窗口图标句柄
    wc.hCursor = nullptr;//鼠标句柄
    wc.hbrBackground = nullptr;//绘制窗口背景的画刷句柄
    wc.lpszMenuName = nullptr;	//窗口菜单的资源id字符串
    wc.lpszClassName = TEXT("mainWindow");//窗口类的名称

    RegisterClass(&wc);

    //创建窗口
    HWND hWnd = CreateWindowEx(0, TEXT("mainWindow"), TEXT("window"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    //初始化OpenGL
    if (!CreateDeviceWGL(hWnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hWnd, &g_MainWindow);
        ::DestroyWindow(hWnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

    //显示窗口
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    //设置 imgui上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // 键盘控制
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Gamepad
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // Multi-Viewport / Platform Windows

    //设置窗口风格
    ImGui::StyleColorsDark();

    //ImGuiConfigFlags_ViewportsEnable 启用后，调整WindowRounding/WindowBg 使平台窗口看起来与常规窗口相同
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    //设置平台/渲染器后端-opengl
    ImGui_ImplWin32_InitForOpenGL(hWnd);
    ImGui_ImplOpenGL3_Init();

    //通过hook连接Win32和GLapi。
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        IM_ASSERT(platform_io.Renderer_CreateWindow == nullptr);
        IM_ASSERT(platform_io.Renderer_DestroyWindow == nullptr);
        IM_ASSERT(platform_io.Renderer_SwapBuffers == nullptr);
        IM_ASSERT(platform_io.Platform_RenderWindow == nullptr);
        platform_io.Renderer_CreateWindow = Hook_Renderer_CreateWindow;
        platform_io.Renderer_DestroyWindow = Hook_Renderer_DestroyWindow;
        platform_io.Renderer_SwapBuffers = Hook_Renderer_SwapBuffers;
        platform_io.Platform_RenderWindow = Hook_Platform_RenderWindow;
    }
    ImVec4 clear_color = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    //消息循环
    bool done = false;
    while (!done)
    {
        MSG nMsg = { 0 };
        //轮询和处理消息
        while (::PeekMessage(&nMsg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&nMsg);        //翻译键盘消息
            DispatchMessage(&nMsg);         //派发消息，将消息交给窗口处理函数
            if (nMsg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        //imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        //显示窗口
        static int counter = 0;
        ImGui::Begin("window!");
        ImGui::Text("helloworld.");
        if (ImGui::Button("Button"))
            counter++;
        ImGui::Text("counter = %d", counter);
        
       /* bool bShowDemoWindow = true;
        ImGui::ShowDemoWindow(&bShowDemoWindow);*/
        ImGui::End();

        //渲染
        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        //更新渲染 windows平台
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            //将OpenGL渲染上下文恢复到主窗口DC，因为平台窗口可能已经更改了它。
            wglMakeCurrent(g_MainWindow.hDC, g_hRC);
        }

        ::SwapBuffers(g_MainWindow.hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hWnd, &g_MainWindow);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hWnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}