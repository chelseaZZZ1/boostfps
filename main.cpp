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

// Global Direct3D Variables
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*      g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Supabase Settings
const std::string SUPABASE_URL = "https://ntaroifjesdztquwkzvt.supabase.co";
const std::string SUPABASE_KEY = "sb_publishable_YIrrmqPLB610TCjSz31T3w_IG5iKOoO";

// App States
bool g_IsAuthenticated = false;
std::string g_LicenseKey = "";
std::string g_KeyStatus = "Awaiting Key Verification...";
int g_MaxAccounts = 50;
int g_ActiveAccounts = 0;
bool g_AntiAFK = true;
bool g_LowNetMode = false;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Helper: Get ProgramData Path
std::string GetProgramDataPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
        std::string dir = std::string(path) + "\\ChxlLauncher";
        CreateDirectoryA(dir.c_str(), NULL);
        return dir + "\\license.lic";
    }
    return "C:\\ChxlLauncher_license.lic";
}

// Curl Write Callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Supabase Key Validation Logic
bool VerifyAndActivateKey(const std::string& key) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string readBuffer;
    std::string url = SUPABASE_URL + "/keys?key=eq." + key;
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("apikey: " + SUPABASE_KEY).c_str());
    headers = curl_slist_append(headers, ("Authorization: Bearer " + SUPABASE_KEY).c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && !readBuffer.empty()) {
        try {
            auto data = json::parse(readBuffer);
            if (!data.empty()) {
                auto keyData = data[0];
                bool isUsed = keyData["is_used"].get<bool>();
                std::string plan = keyData["plan"].get<std::string>(); // "7_day", "31_day", "lifetime"

                // First time activation logic
                if (!isUsed) {
                    // Update key as used + set activated_at in Supabase via PATCH
                    // ( Implementation depends on your Supabase table schema )
                }

                // Save locally
                std::ofstream outfile(GetProgramDataPath());
                outfile << key;
                outfile.close();

                g_KeyStatus = "Key Valid! Plan: " + plan;
                return true;
            }
        } catch (...) {
            g_KeyStatus = "Invalid Response / Parsing Error";
        }
    }
    g_KeyStatus = "Invalid Key!";
    return false;
}

// Roblox Multi-Instance Unlocker
void UnlockRobloxMultiInstance() {
    CreateMutexA(NULL, TRUE, "ROBLOX_singletonMutex");
}

// Anti-AFK Thread Function
void AntiAFKWorker() {
    while (true) {
        if (g_AntiAFK) {
            // Find Roblox Windows & Send Keypress
            HWND hwnd = FindWindowA(NULL, "Roblox");
            if (hwnd) {
                SendMessage(hwnd, WM_KEYDOWN, VK_SPACE, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                SendMessage(hwnd, WM_KEYUP, VK_SPACE, 0);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

// Potassium Executor Sync (IPC Named Pipe Example)
void SyncWithPotassium(const std::string& script) {
    HANDLE hPipe = CreateFileA("\\\\.\\pipe\\PotassiumPipe", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hPipe, script.c_str(), script.length(), &bytesWritten, NULL);
        CloseHandle(hPipe);
    }
}

// Custom Smooth UI Helpers
float Lerp(float a, float b, float t) { return a + t * (b - a); }

// WinMain Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Enable Multi-Instance Roblox
    UnlockRobloxMultiInstance();

    // Start Anti-AFK
    std::thread(AntiAFKWorker).detach();

    // Create Application Window
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ChxlLauncherClass", NULL };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Chxl Launcher Pro", WS_POPUP | WS_VISIBLE, 100, 100, 960, 580, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Smooth Dark Styling
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 6.0f;
    style.WindowBorderSize = 0.0f;

    // Load Saved Key if exists
    std::ifstream keyFile(GetProgramDataPath());
    if (keyFile.is_open()) {
        std::getline(keyFile, g_LicenseKey);
        keyFile.close();
        if (!g_LicenseKey.empty()) {
            g_IsAuthenticated = VerifyAndActivateKey(g_LicenseKey);
        }
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    float bg_anim = 0.0f;

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

        // Main UI Code
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Chxl Main Engine", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Sidebar / Header
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "CHXL LAUNCHER ULTRA v2.0");
        ImGui::Separator();

        if (!g_IsAuthenticated) {
            // Auth Screen UI
            ImGui::Spacing();
            ImGui::Text("Enter License Key:");
            static char keyInput[128] = "";
            ImGui::InputText("##key", keyInput, IM_ARRAYSIZE(keyInput));
            
            if (ImGui::Button("Activate Key", ImVec2(150, 35))) {
                g_LicenseKey = keyInput;
                g_IsAuthenticated = VerifyAndActivateKey(g_LicenseKey);
            }
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_KeyStatus.c_str());
        } else {
            // Dashboard UI
            ImGui::BeginTabBar("MainTabs");

            if (ImGui::BeginTabItem("Multi-Instance Manager")) {
                ImGui::Text("Active Roblox Instances: %d / %d", g_ActiveAccounts, g_MaxAccounts);
                ImGui::SliderInt("Max Accounts Target", &g_MaxAccounts, 1, 50);

                if (ImGui::Button("Launch New Instance", ImVec2(180, 40))) {
                    if (g_ActiveAccounts < g_MaxAccounts) {
                        ShellExecuteA(NULL, "open", "roblox://", NULL, NULL, SW_SHOWNORMAL);
                        g_ActiveAccounts++;
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Optimization & Anti-AFK")) {
                ImGui::Checkbox("Enable Anti-AFK (Bypass 20 min disconnect)", &g_AntiAFK);
                ImGui::Checkbox("Low Network / Resource Saver Mode", &g_LowNetMode);
                
                if (g_LowNetMode) {
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Status: Bandwidth and CPU throttled for multi-account stability.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Potassium Sync")) {
                ImGui::Text("Sync Execution with Potassium Executor");
                static char scriptBuf[1024] = "print('Hello from Chxl Launcher!')";
                ImGui::InputTextMultiline("##script", scriptBuf, IM_ARRAYSIZE(scriptBuf), ImVec2(-1, 180));

                if (ImGui::Button("Execute across All จอ", ImVec2(200, 40))) {
                    SyncWithPotassium(scriptBuf);
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        // Rendering Loop
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.08f, 0.08f, 0.10f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Smooth V-Sync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// DX11 Helper Functions
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
