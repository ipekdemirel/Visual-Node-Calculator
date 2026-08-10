#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "NodeEditor.h"

#pragma comment(lib, "d3d11.lib")

// ------------------------------------------------------------
// DirectX değişkenleri
// ------------------------------------------------------------

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// ------------------------------------------------------------
// Fonksiyon bildirimleri
// ------------------------------------------------------------

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();

void CreateRenderTarget();
void CleanupRenderTarget();

LRESULT WINAPI WndProc(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

// ------------------------------------------------------------
// Programın başlangıç noktası
// ------------------------------------------------------------

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int
)
{
    // --------------------------------------------------------
    // Windows penceresini oluşturuyoruz
    // --------------------------------------------------------

    WNDCLASSEXW wc = {};

    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VisualNodeCalculatorWindow";

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"Visual Node Calculator",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        1400,
        850,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    // --------------------------------------------------------
    // DirectX 11 başlatılıyor
    // --------------------------------------------------------

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();

        UnregisterClassW(
            wc.lpszClassName,
            wc.hInstance
        );

        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // --------------------------------------------------------
    // Dear ImGui başlatılıyor
    // --------------------------------------------------------

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // --------------------------------------------------------
    // ImGui teması
    // --------------------------------------------------------

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;

    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);

    style.Colors[ImGuiCol_WindowBg] =
        ImVec4(0.025f, 0.028f, 0.040f, 1.0f);

    style.Colors[ImGuiCol_ChildBg] =
        ImVec4(0.030f, 0.034f, 0.050f, 1.0f);

    style.Colors[ImGuiCol_FrameBg] =
        ImVec4(0.075f, 0.085f, 0.120f, 1.0f);

    style.Colors[ImGuiCol_FrameBgHovered] =
        ImVec4(0.110f, 0.130f, 0.180f, 1.0f);

    style.Colors[ImGuiCol_FrameBgActive] =
        ImVec4(0.140f, 0.160f, 0.220f, 1.0f);

    style.Colors[ImGuiCol_Button] =
        ImVec4(0.110f, 0.180f, 0.300f, 1.0f);

    style.Colors[ImGuiCol_ButtonHovered] =
        ImVec4(0.150f, 0.260f, 0.440f, 1.0f);

    style.Colors[ImGuiCol_ButtonActive] =
        ImVec4(0.180f, 0.310f, 0.520f, 1.0f);

    style.Colors[ImGuiCol_Header] =
        ImVec4(0.120f, 0.180f, 0.300f, 1.0f);

    style.Colors[ImGuiCol_HeaderHovered] =
        ImVec4(0.170f, 0.260f, 0.430f, 1.0f);

    style.Colors[ImGuiCol_HeaderActive] =
        ImVec4(0.190f, 0.300f, 0.500f, 1.0f);

    // --------------------------------------------------------
    // ImGui, Windows ve DirectX bağlantısı
    // --------------------------------------------------------

    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX11_Init(
        g_pd3dDevice,
        g_pd3dDeviceContext
    );

    // --------------------------------------------------------
    // ImNodes sistemini bir kez başlatıyoruz
    // --------------------------------------------------------

    NodeEditor::Initialize();

    bool done = false;

    // --------------------------------------------------------
    // Ana program döngüsü
    // --------------------------------------------------------

    while (!done)
    {
        MSG msg;

        while (PeekMessage(
            &msg,
            nullptr,
            0U,
            0U,
            PM_REMOVE
        ))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                done = true;
            }
        }

        if (done)
        {
            break;
        }

        // ----------------------------------------------------
        // Yeni ImGui karesi
        // ----------------------------------------------------

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ----------------------------------------------------
        // Node Editor ekranını çiziyoruz
        // ----------------------------------------------------

        NodeEditor::Draw();

        // ----------------------------------------------------
        // ImGui çizimini tamamlıyoruz
        // ----------------------------------------------------

        ImGui::Render();

        const float clearColor[4] =
        {
            0.010f,
            0.012f,
            0.020f,
            1.0f
        };

        g_pd3dDeviceContext->OMSetRenderTargets(
            1,
            &g_mainRenderTargetView,
            nullptr
        );

        g_pd3dDeviceContext->ClearRenderTargetView(
            g_mainRenderTargetView,
            clearColor
        );

        ImGui_ImplDX11_RenderDrawData(
            ImGui::GetDrawData()
        );

        g_pSwapChain->Present(1, 0);
    }

    // --------------------------------------------------------
    // Program kapanırken temizleme
    // --------------------------------------------------------

    NodeEditor::Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    DestroyWindow(hwnd);

    UnregisterClassW(
        wc.lpszClassName,
        wc.hInstance
    );

    return 0;
}

// ------------------------------------------------------------
// DirectX 11 cihazını oluşturur
// ------------------------------------------------------------

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};

    sd.BufferCount = 2;

    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;

    sd.BufferDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;

    sd.Flags =
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    sd.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    sd.OutputWindow = hWnd;

    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;

    sd.Windowed = TRUE;

    sd.SwapEffect =
        DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;

    D3D_FEATURE_LEVEL featureLevel;

    const D3D_FEATURE_LEVEL featureLevelArray[2] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT result =
        D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext
        );

    if (result == DXGI_ERROR_UNSUPPORTED)
    {
        result =
            D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                createDeviceFlags,
                featureLevelArray,
                2,
                D3D11_SDK_VERSION,
                &sd,
                &g_pSwapChain,
                &g_pd3dDevice,
                &featureLevel,
                &g_pd3dDeviceContext
            );
    }

    if (result != S_OK)
    {
        return false;
    }

    CreateRenderTarget();

    return true;
}

// ------------------------------------------------------------
// DirectX kaynaklarını temizler
// ------------------------------------------------------------

void CleanupDeviceD3D()
{
    CleanupRenderTarget();

    if (g_pSwapChain != nullptr)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }

    if (g_pd3dDeviceContext != nullptr)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }

    if (g_pd3dDevice != nullptr)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

// ------------------------------------------------------------
// Çizim hedefini oluşturur
// ------------------------------------------------------------

void CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;

    g_pSwapChain->GetBuffer(
        0,
        IID_PPV_ARGS(&backBuffer)
    );

    if (backBuffer != nullptr)
    {
        g_pd3dDevice->CreateRenderTargetView(
            backBuffer,
            nullptr,
            &g_mainRenderTargetView
        );

        backBuffer->Release();
    }
}

// ------------------------------------------------------------
// Çizim hedefini temizler
// ------------------------------------------------------------

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView != nullptr)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

// ------------------------------------------------------------
// ImGui Windows mesaj işleyicisi
// ------------------------------------------------------------

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

// ------------------------------------------------------------
// Windows mesajlarını işler
// ------------------------------------------------------------

LRESULT WINAPI WndProc(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (ImGui_ImplWin32_WndProcHandler(
        hWnd,
        msg,
        wParam,
        lParam
    ))
    {
        return true;
    }

    switch (msg)
    {
    case WM_SIZE:

        if (
            wParam != SIZE_MINIMIZED &&
            g_pd3dDevice != nullptr
            )
        {
            CleanupRenderTarget();

            g_pSwapChain->ResizeBuffers(
                0,
                static_cast<UINT>(LOWORD(lParam)),
                static_cast<UINT>(HIWORD(lParam)),
                DXGI_FORMAT_UNKNOWN,
                0
            );

            CreateRenderTarget();
        }

        return 0;

    case WM_SYSCOMMAND:

        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }

        break;

    case WM_DESTROY:

        PostQuitMessage(0);

        return 0;
    }

    return DefWindowProcW(
        hWnd,
        msg,
        wParam,
        lParam
    );
}