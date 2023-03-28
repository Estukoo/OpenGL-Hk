#include "Hooks.h"

static BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    const auto isMainWindow = [handle]() {
        return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle);
    };

    DWORD pID = 0;
    GetWindowThreadProcessId(handle, &pID);

    if (GetCurrentProcessId() != pID || !isMainWindow() || handle == GetConsoleWindow())
        return TRUE;

    *reinterpret_cast<HWND*>(lParam) = handle;

    return FALSE;
}

HWND GetProcessWindow()
{
    HWND hwnd = nullptr;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));

    while (!hwnd) {
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    char name[128];
    GetWindowTextA(hwnd, name, RTL_NUMBER_OF(name));

    return hwnd;
}

Hooks::Hooks()
{
    // Hook WndProc
    hWnd = GetProcessWindow();
    oWndProc = (WNDPROC)SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

    MH_Initialize();

    // Hook OpenGL
    swapBuffers = (void*)GetProcAddress(GetModuleHandle(L"opengl32.dll"), "wglSwapBuffers");
    std::cout << "[+] SwapBuffers addr : " << (uintptr_t)swapBuffers << std::endl;

    MH_CreateHook(swapBuffers, &wglSwapBuffers, (LPVOID*)&oWglSwapBuffers);
    MH_EnableHook(swapBuffers);
    std::cout << "[+] Hooked OpengGL! Origin : " << (uintptr_t)oWglSwapBuffers << std::endl;
}

void Hooks::Remove()
{
    SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);

    gui->Remove();
    MH_RemoveHook(swapBuffers);
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall Hooks::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN && wParam == VK_INSERT)
        gui->draw = !gui->draw;

    if (gui && gui->draw && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        std::cout << "ImGui_ImplWin32_WndProcHandler handled message: " << msg << std::endl;
        return true;
    }

    char windowTitle[256];
    GetWindowTextA(hWnd, windowTitle, sizeof(windowTitle));
    std::cout << windowTitle << std::endl;

    LRESULT result = CallWindowProc(hooks->oWndProc, hWnd, msg, wParam, lParam);
    // std::cout << "WndProc received message: " << msg << ", result: " << result << std::endl;

    if (ImGui::GetCurrentContext() != nullptr) {
        std::cout << "ImGui Initialized" << std::endl;
    }
    else {
        std::cout << "ImGui Not Initialized" << std::endl;
    }

    return result;
}

bool __stdcall Hooks::wglSwapBuffers(HDC hDc)
{
    static HGLRC oContext = wglGetCurrentContext();
    static HGLRC newContext = wglCreateContext(hDc);

    static bool init = false;
    if (!init) {
        newContext = wglCreateContext(hDc);
        wglMakeCurrent(hDc, newContext);

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glViewport(0, 0, viewport[2], viewport[3]);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, viewport[2], viewport[3], 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);

        gui = std::make_unique<GUI>(hooks->hWnd, ImGui::CreateContext());

        init = true;
    }
    else {
        wglMakeCurrent(hDc, newContext);
        gui->Draw();
    }

    wglMakeCurrent(hDc, oContext);

    return hooks->oWglSwapBuffers(hDc);
}