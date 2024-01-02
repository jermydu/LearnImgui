#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <GL/GL.h>

//Windows ��������
struct WGL_WindowData { HDC hDC; };
static HGLRC              g_hRC;          //opengl ���
static WGL_WindowData     g_MainWindow;     //���ھ��
static int              g_Width;
static int              g_Height;

//��ʼ��opengl
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

//�ͷ�opengl
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

//���ڴ��������Զ��塢������Ϣ��
LRESULT APIENTRY WndProc(HWND hWnd, UINT msgID, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msgID, wParam, lParam))
        return true;

    //������Ϣ
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
        if ((wParam & 0xfff0) == SC_KEYMENU) //����ALTӦ�ó���˵�
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

    //��������ϢĬ�ϴ���
    return DefWindowProc(hWnd, msgID, wParam, lParam);
}

int main(int argc, char* argv[])
{

    //ע�ᴰ����
    WNDCLASS wc = { 0 };
    wc.style = CS_OWNDC;//������ķ��
    wc.lpfnWndProc = WndProc;//���ڴ�����
    wc.cbClsExtra = 0;//������ĸ�������buff�Ĵ�С			
    wc.cbWndExtra = 0;//���ڵĸ�������buff�Ĵ�С
    wc.hInstance = GetModuleHandle(nullptr);//��ǰģ��ʵ�����
    wc.hIcon = nullptr;//����ͼ����
    wc.hCursor = nullptr;//�����
    wc.hbrBackground = nullptr;//���ƴ��ڱ����Ļ�ˢ���
    wc.lpszMenuName = nullptr;	//���ڲ˵�����Դid�ַ���
    wc.lpszClassName = TEXT("mainWindow");//�����������

    RegisterClass(&wc);

    //��������
    HWND hWnd = CreateWindowEx(0, TEXT("mainWindow"), TEXT("window"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    //��ʼ��OpenGL
    if (!CreateDeviceWGL(hWnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hWnd, &g_MainWindow);
        ::DestroyWindow(hWnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

    //��ʾ����
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    //���� imgui������
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // ���̿���
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Gamepad
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // Multi-Viewport / Platform Windows

    //���ô��ڷ��
    ImGui::StyleColorsDark();

    //ImGuiConfigFlags_ViewportsEnable ���ú󣬵���WindowRounding/WindowBg ʹƽ̨���ڿ������볣�洰����ͬ
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    //����ƽ̨/��Ⱦ�����-opengl
    ImGui_ImplWin32_InitForOpenGL(hWnd);
    ImGui_ImplOpenGL3_Init();

    //ͨ��hook����Win32��GLapi��
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

    //��Ϣѭ��
    bool done = false;
    while (!done)
    {
        MSG nMsg = { 0 };
        //��ѯ�ʹ�����Ϣ
        while (::PeekMessage(&nMsg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&nMsg);        //���������Ϣ
            DispatchMessage(&nMsg);         //�ɷ���Ϣ������Ϣ�������ڴ�����
            if (nMsg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        //imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        //��ʾ����
        static int counter = 0;
        ImGui::Begin("window!");
        ImGui::Text("helloworld.");
        if (ImGui::Button("Button"))
            counter++;
        ImGui::Text("counter = %d", counter);
        
       /* bool bShowDemoWindow = true;
        ImGui::ShowDemoWindow(&bShowDemoWindow);*/
        ImGui::End();

        //��Ⱦ
        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        //������Ⱦ windowsƽ̨
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            //��OpenGL��Ⱦ�����Ļָ���������DC����Ϊƽ̨���ڿ����Ѿ�����������
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