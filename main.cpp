#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "libcurl.lib")

#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <shlobj.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

using json = nlohmann::json;

// Direct3D 11 Global Variables
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*      g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Supabase Settings
const std::string SUPABASE_URL = "https://ntaroifjesdztquwkzvt.supabase.co";
const std::string SUPABASE_KEY = "sb_publishable_YIrrmqPLB610TCjSz31T3w_IG5iKOoO";

// Application State
bool g_IsAuthenticated = false;
std::string g_LicenseKey = "";
std::string g_KeyStatus = "Ready to authenticate";

// Customization & UI State
enum UIMode { LOADING, SELECT_UI, CLASSIC_UI, NEO_MODERN_UI };
UIMode g_CurrentUIMode = LOADING;
float g_LoadingProgress = 0.0f;
std::string g_LoadingStatus = "Initializing Core Systems...";

// Roblox Account Instance Manager Structure
struct RobloxInstance {
    int id;
    std::string accountName;
    HWND hwnd;
    bool isConnected;
    bool antiAFK;
    bool lowResourceMode;
    int fpsCap;
    std::string currentStatus;
};

std::vector<RobloxInstance> g_Instances;
int g_SelectedInstanceIndex = -1;

// Forward Declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Utility: Bypass Roblox Single-Instance Mutex
void UnlockRobloxMultiInstance() {
    CreateMutexA(NULL, TRUE, "ROBLOX_singletonMutex");
}

// Get ProgramData License File Path
std::string GetProgramDataPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
        std::string dir = std::string(path) + "\\ChxlLauncher";
        CreateDirectoryA(dir.c_str(), NULL);
        return dir + "\\license.lic";
    }
    return "C:\\ChxlLauncher_license.lic";
}

// Curl Callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Verify Key via Supabase RPC
bool VerifyAndActivateKey(const std::string& key) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string readBuffer;
    std::string url = SUPABASE_URL + "/rest/v1/rpc/check_and_activate_key";
    
    json payload;
    payload["user_key"] = key;
    std::string jsonStr = payload.dump();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
    headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res == CURLE_OK && !readBuffer.empty()) {
        try {
            auto resJson = json::parse(readBuffer);
            if (resJson.contains("valid") && resJson["valid"].get<bool>()) {
                std::ofstream outfile(GetProgramDataPath());
                outfile << key;
                outfile.close();
                g_KeyStatus = "Active (" + resJson.value("plan", "Lifetime") + ")";
                return true;
            } else {
                g_KeyStatus = resJson.value("message", "Invalid License Key!");
            }
        } catch (...) {
            g_KeyStatus = "Response Parsing Error";
        }
    } else {
        g_KeyStatus = "Connection Failed";
    }
    return false;
}

// Real Windows Enumeration for Active Roblox Windows
BOOL CALLBACK EnumRobloxWindows(HWND hwnd, LPARAM lParam) {
    char class_name[80];
    char title[128];
    GetClassNameA(hwnd, class_name, sizeof(class_name));
    GetWindowTextA(hwnd, title, sizeof(title));

    if (std::string(class_name) == "ApplicationFrameWindow" || std::string(class_name) == "RobloxAppClass") {
        if (std::string(title).find("Roblox") != std::string::npos) {
            bool exists = false;
            for (auto& inst : g_Instances) {
                if (inst.hwnd == hwnd) { exists = true; break; }
            }
            if (!exists) {
                RobloxInstance inst;
                inst.id = (int)g_Instances.size() + 1;
                inst.accountName = "Roblox Account #" + std::to_string(inst.id);
                inst.hwnd = hwnd;
                inst.isConnected = true;
                inst.antiAFK = true;
                inst.lowResourceMode = false;
                inst.fpsCap = 60;
                inst.currentStatus = "Active & Running";
                g_Instances.push_back(inst);
            }
        }
    }
    return TRUE;
}

void RefreshRobloxInstances() {
    EnumWindows(EnumRobloxWindows, 0);
}

// Send Script Injection via Pipe to Specific Window
void SendScriptToPipe(const std::string& pipeName, const std::string& script) {
    HANDLE hPipe = CreateFileA(pipeName.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hPipe, script.c_str(), (DWORD)script.length(), &bytesWritten, NULL);
        CloseHandle(hPipe);
    }
}

// Dynamic UI Styling Configurator
void ApplyNeoModernStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 16.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.05f, 0.05f, 0.08f, 0.94f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.09f, 0.09f, 0.13f, 0.60f);
    colors[ImGuiCol_Border]             = ImVec4(0.25f, 0.25f, 0.40f, 0.40f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.12f, 0.12f, 0.18f, 0.80f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.30f, 0.80f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.28f, 0.28f, 0.42f, 0.80f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.06f, 0.06f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.10f, 0.10f, 0.16f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.38f, 0.28f, 0.85f, 0.70f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.48f, 0.38f, 0.95f, 0.90f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.58f, 0.48f, 1.00f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.20f, 0.18f, 0.35f, 0.60f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.30f, 0.28f, 0.50f, 0.80f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.40f, 0.38f, 0.65f, 1.00f);
}

void ApplyClassicStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.WindowBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Border]             = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.24f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.30f, 0.50f, 0.88f, 1.00f);
}

// ---------------------------------------------------------
// RENDER UI MODES
// ---------------------------------------------------------

void RenderLoadingScreen() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("LoadingScreen", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.4f);

    ImGui::SetCursorPos(ImVec2(center.x - 180, center.y - 60));
    ImGui::TextColored(ImVec4(0.5f, 0.4f, 1.0f, 1.0f), "CHXL LAUNCHER ENGINE v3.0");

    ImGui::SetCursorPos(ImVec2(center.x - 180, center.y - 20));
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "%s", g_LoadingStatus.c_str());

    ImGui::SetCursorPos(ImVec2(center.x - 200, center.y + 20));
    ImGui::ProgressBar(g_LoadingProgress, ImVec2(400, 20));

    // Simulate Step Loading Progress
    g_LoadingProgress += 0.008f;
    if (g_LoadingProgress > 0.3f && g_LoadingProgress < 0.6f) {
        g_LoadingStatus = "Bypassing Multi-Instance Restrictions...";
        UnlockRobloxMultiInstance();
    } else if (g_LoadingProgress >= 0.6f && g_LoadingProgress < 0.9f) {
        g_LoadingStatus = "Connecting to Supabase License Server...";
    } else if (g_LoadingProgress >= 1.0f) {
        std::ifstream keyFile(GetProgramDataPath());
        if (keyFile.is_open()) {
            std::getline(keyFile, g_LicenseKey);
            keyFile.close();
            if (!g_LicenseKey.empty()) {
                g_IsAuthenticated = VerifyAndActivateKey(g_LicenseKey);
            }
        }
        g_CurrentUIMode = SELECT_UI;
    }

    ImGui::End();
}

void RenderUISelectionScreen() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("UISelection", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    ImVec2 size = ImGui::GetIO().DisplaySize;

    ImGui::SetCursorPos(ImVec2(size.x * 0.5f - 160, 60));
    ImGui::TextColored(ImVec4(1, 1, 1, 1), "CHOOSE YOUR PREFERRED INTERFACE");

    ImGui::SetCursorPos(ImVec2(size.x * 0.2f, size.y * 0.3f));
    if (ImGui::Button("EASY CLASSIC UI\n\n- Simple Layout\n- Low Resource Usage\n- High Performance", ImVec2(240, 220))) {
        ApplyClassicStyle();
        g_CurrentUIMode = CLASSIC_UI;
    }

    ImGui::SetCursorPos(ImVec2(size.x * 0.6f, size.y * 0.3f));
    if (ImGui::Button("NEO MODERN UI (RECOMMENDED)\n\n- Multi-Instance Split Control\n- Advanced Anti-AFK & FPS Saver\n- Potassium Integration\n- Beautiful Glassmorphism", ImVec2(260, 220))) {
        ApplyNeoModernStyle();
        g_CurrentUIMode = NEO_MODERN_UI;
    }

    ImGui::End();
}

void RenderNeoModernUI() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("NeoMain", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // Header Bar
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 1.0f, 1.0f), "CHXL ULTRA MULTI-MANAGER");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[ License: %s ]", g_KeyStatus.c_str());
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 120);
    if (ImGui::Button("Switch UI", ImVec2(100, 25))) {
        g_CurrentUIMode = SELECT_UI;
    }
    ImGui::Separator();

    if (!g_IsAuthenticated) {
        ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.3f, 150));
        ImGui::BeginChild("AuthBox", ImVec2(400, 200), true);
        ImGui::Text("ENTER LICENSE KEY TO UNLOCK:");
        static char keyBuf[128] = "";
        ImGui::InputText("##keyin", keyBuf, IM_ARRAYSIZE(keyBuf));
        if (ImGui::Button("Activate License", ImVec2(-1, 35))) {
            g_LicenseKey = keyBuf;
            g_IsAuthenticated = VerifyAndActivateKey(g_LicenseKey);
        }
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_KeyStatus.c_str());
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    // Left Panel: Instances List
    ImGui::BeginChild("InstanceList", ImVec2(280, 0), true);
    ImGui::Text("ACTIVE ROBLOX WINDOWS");
    if (ImGui::Button("Refresh List", ImVec2(-1, 30))) {
        RefreshRobloxInstances();
    }
    ImGui::Separator();

    for (int i = 0; i < (int)g_Instances.size(); i++) {
        std::string label = g_Instances[i].accountName + "##" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), g_SelectedInstanceIndex == i, 0, ImVec2(0, 30))) {
            g_SelectedInstanceIndex = i;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel: Specific Instance Detailed Control
    ImGui::BeginChild("InstanceDetails", ImVec2(0, 0), true);
    if (g_SelectedInstanceIndex >= 0 && g_SelectedInstanceIndex < (int)g_Instances.size()) {
        auto& inst = g_Instances[g_SelectedInstanceIndex];

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CONTROLLING: %s", inst.accountName.c_str());
        ImGui::Separator();

        ImGui::Checkbox("Enable Anti-AFK (20 min Bypass)", &inst.antiAFK);
        ImGui::Checkbox("Low Resource Saver (Freeze Unfocused Render)", &inst.lowResourceMode);
        ImGui::SliderInt("Target FPS Limit", &inst.fpsCap, 15, 240);

        ImGui::Spacing();
        ImGui::Text("QUICK WINDOW ACTIONS:");
        if (ImGui::Button("Bring to Front / Focus", ImVec2(180, 35))) {
            SetForegroundWindow(inst.hwnd);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close This Instance", ImVec2(180, 35))) {
            PostMessage(inst.hwnd, WM_CLOSE, 0, 0);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("POTASSIUM SCRIPT INJECTOR (THIS SCREEN ONLY):");
        static char scriptBuffer[2048] = "print('Executed on selected instance!')";
        ImGui::InputTextMultiline("##single_script", scriptBuffer, IM_ARRAYSIZE(scriptBuffer), ImVec2(-1, 140));

        if (ImGui::Button("Execute Script on Selected จอ", ImVec2(-1, 40))) {
            SendScriptToPipe("\\\\.\\pipe\\PotassiumPipe", scriptBuffer);
        }
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an active Roblox Instance from the left list to open controls.");
    }
    ImGui::EndChild();

    ImGui::End();
}

void RenderClassicUI() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Classic Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    ImGui::Text("CHXL LAUNCHER - CLASSIC EASY MODE");
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 100);
    if (ImGui::Button("Switch UI")) g_CurrentUIMode = SELECT_UI;
    ImGui::Separator();

    if (ImGui::Button("Launch New Roblox จอ", ImVec2(200, 40))) {
        ShellExecuteA(NULL, "open", "roblox://", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto Anti-AFK All จอ", ImVec2(200, 40))) {
        // Toggle Anti AFK for All
    }

    ImGui::Text("Running Roblox Accounts Count: %d", (int)g_Instances.size());
    ImGui::End();
}

// Main Window Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ChxlUltraClass", NULL };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Chxl Launcher Ultra", WS_POPUP | WS_VISIBLE, 100, 100, 1000, 620, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Load High Quality System Font
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render Active Mode
        switch (g_CurrentUIMode) {
            case LOADING:          RenderLoadingScreen(); break;
            case SELECT_UI:        RenderUISelectionScreen(); break;
            case NEO_MODERN_UI:    RenderNeoModernUI(); break;
            case CLASSIC_UI:       RenderClassicUI(); break;
        }

        ImGui::Render();
        const float clear_color[4] = { 0.04f, 0.04f, 0.06f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
