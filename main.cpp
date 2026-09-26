#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "libcurl.lib")

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <tchar.h>
#include <tlhelp32.h>
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
bool g_IsAuthenticated = true; // Auto-Authenticated
std::string g_LicenseKey = "";
std::string g_KeyStatus = "Active (lifetime)";

// Customization & UI State
enum UIMode { LOADING, SELECT_UI, CLASSIC_UI, NEO_MODERN_UI };
UIMode g_CurrentUIMode = LOADING;
float g_LoadingProgress = 0.0f;
std::string g_LoadingStatus = "Initializing Core Engines...";

// Roblox Account Instance Manager Structure
struct RobloxInstance {
    int id;
    DWORD pid;
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

// ---------------------------------------------------------
// REAL ROBLOX PROCESS & WINDOW ENUMERATION SYSTEM
// ---------------------------------------------------------

struct EnumData {
    DWORD processId;
    HWND hWnd;
};

BOOL CALLBACK EnumProcForPID(HWND hwnd, LPARAM lParam) {
    EnumData* data = (EnumData*)lParam;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == data->processId && IsWindowVisible(hwnd)) {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        // เลือกหน้าต่างหลักที่มีขนาดของจอเกมจริงๆ
        if ((rc.right - rc.left) > 200 && (rc.bottom - rc.top) > 200) {
            data->hWnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

HWND GetHwndFromProcessId(DWORD pid) {
    EnumData data = { pid, NULL };
    EnumWindows(EnumProcForPID, (LPARAM)&data);
    return data.hWnd;
}

void RefreshRobloxInstances() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            // ค้นหาจาก Process RobloxPlayerBeta.exe โดยตรง
            if (_wcsicmp(pe32.szExeFile, L"RobloxPlayerBeta.exe") == 0) {
                DWORD pid = pe32.th32ProcessID;
                HWND hwnd = GetHwndFromProcessId(pid);

                bool exists = false;
                for (auto& inst : g_Instances) {
                    if (inst.pid == pid) {
                        exists = true;
                        inst.hwnd = hwnd; // อัปเดต HWND
                        break;
                    }
                }

                if (!exists) {
                    RobloxInstance inst;
                    inst.id = (int)g_Instances.size() + 1;
                    inst.pid = pid;
                    inst.accountName = "Roblox Account (PID: " + std::to_string(pid) + ")";
                    inst.hwnd = hwnd;
                    inst.isConnected = true;
                    inst.antiAFK = true;
                    inst.lowResourceMode = false;
                    inst.fpsCap = 60;
                    inst.currentStatus = "Active & Running";
                    g_Instances.push_back(inst);
                }
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);

    if (g_SelectedInstanceIndex == -1 && !g_Instances.empty()) {
        g_SelectedInstanceIndex = 0;
    }
}

// Send Script Injection via Pipe
void SendScriptToPipe(const std::string& pipeName, const std::string& script) {
    HANDLE hPipe = CreateFileA(pipeName.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hPipe, script.c_str(), (DWORD)script.length(), &bytesWritten, NULL);
        CloseHandle(hPipe);
    }
}

// Styling Setup
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
    colors[ImGuiCol_WindowBg]           = ImVec4(0.05f, 0.05f, 0.08f, 0.96f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.09f, 0.09f, 0.13f, 0.70f);
    colors[ImGuiCol_Border]             = ImVec4(0.30f, 0.25f, 0.50f, 0.50f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.12f, 0.12f, 0.18f, 0.80f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.32f, 0.90f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.28f, 0.28f, 0.45f, 1.00f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.06f, 0.06f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.10f, 0.10f, 0.16f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.38f, 0.28f, 0.85f, 0.75f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.48f, 0.38f, 0.95f, 0.95f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.58f, 0.48f, 1.00f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.25f, 0.20f, 0.45f, 0.60f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.35f, 0.28f, 0.60f, 0.85f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.45f, 0.38f, 0.75f, 1.00f);
}

void ApplyClassicStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_Border]             = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.24f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.30f, 0.50f, 0.88f, 1.00f);
}

// ---------------------------------------------------------
// RENDER UI MODES WITH SMOOTH ANIMATIONS & DRAG CONTROL
// ---------------------------------------------------------

void RenderTopBar(HWND hwnd) {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    
    // Custom Titlebar Drag Area
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##titlebardrag", ImVec2(displaySize.x - 90, 35));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        POINT p;
        GetCursorPos(&p);
        SetWindowPos(hwnd, NULL, p.x - (int)displaySize.x / 2, p.y - 15, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    // Window Controls (Minimize / Close)
    ImGui::SetCursorPos(ImVec2(displaySize.x - 85, 5));
    if (ImGui::Button("-", ImVec2(35, 25))) {
        ShowWindow(hwnd, SW_MINIMIZE);
    }
    ImGui::SameLine();
    if (ImGui::Button("X", ImVec2(35, 25))) {
        PostQuitMessage(0);
    }
}

void RenderLoadingScreen(HWND hwnd) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("LoadingScreen", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    RenderTopBar(hwnd);

    ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.45f);

    static float timeAcc = 0.0f;
    timeAcc += ImGui::GetIO().DeltaTime * 3.0f;
    float pulseAlpha = (sinf(timeAcc) * 0.2f) + 0.8f;

    ImGui::SetCursorPos(ImVec2(center.x - 210, center.y - 70));
    ImGui::TextColored(ImVec4(0.6f, 0.4f, 1.0f, pulseAlpha), "CHXL LAUNCHER ULTRA ENGINE v3.0");

    ImGui::SetCursorPos(ImVec2(center.x - 210, center.y - 25));
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.9f, 0.9f), "%s", g_LoadingStatus.c_str());

    ImGui::SetCursorPos(ImVec2(center.x - 220, center.y + 20));
    ImGui::ProgressBar(g_LoadingProgress, ImVec2(440, 22));

    g_LoadingProgress += ImGui::GetIO().DeltaTime * 0.6f;
    if (g_LoadingProgress > 0.35f && g_LoadingProgress < 0.7f) {
        g_LoadingStatus = "Unlocking Multi-Instance Restrictions...";
        UnlockRobloxMultiInstance();
    } else if (g_LoadingProgress >= 0.7f && g_LoadingProgress < 0.95f) {
        g_LoadingStatus = "Scanning Active Roblox Processes...";
        RefreshRobloxInstances();
    } else if (g_LoadingProgress >= 1.0f) {
        g_CurrentUIMode = SELECT_UI;
    }

    ImGui::End();
}

void RenderUISelectionScreen(HWND hwnd) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("UISelection", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    RenderTopBar(hwnd);

    ImVec2 size = ImGui::GetIO().DisplaySize;

    ImGui::SetCursorPos(ImVec2(size.x * 0.5f - 180, 50));
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "CHOOSE YOUR PREFERRED INTERFACE");

    float cardWidth = 280.0f;
    float cardHeight = 240.0f;

    ImGui::SetCursorPos(ImVec2(size.x * 0.5f - cardWidth - 20, size.y * 0.3f));
    if (ImGui::Button("EASY CLASSIC UI\n\n- Simple & Clean Layout\n- Minimal CPU & RAM Usage\n- High FPS Performance", ImVec2(cardWidth, cardHeight))) {
        ApplyClassicStyle();
        g_CurrentUIMode = CLASSIC_UI;
    }

    ImGui::SetCursorPos(ImVec2(size.x * 0.5f + 20, size.y * 0.3f));
    if (ImGui::Button("NEO MODERN UI (RECOMMENDED)\n\n- Multi-Instance Split Controls\n- Individual Anti-AFK & FPS Limits\n- Potassium Executor Integration\n- Smooth Animations & Glass UI", ImVec2(cardWidth, cardHeight))) {
        ApplyNeoModernStyle();
        g_CurrentUIMode = NEO_MODERN_UI;
    }

    ImGui::End();
}

void RenderNeoModernUI(HWND hwnd) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("NeoMain", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    RenderTopBar(hwnd);

    // Header Bar Info
    ImGui::SetCursorPos(ImVec2(15, 10));
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 1.0f, 1.0f), "CHXL ULTRA MULTI-MANAGER");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[ License: %s ]", g_KeyStatus.c_str());
    
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 200);
    if (ImGui::Button("Switch UI", ImVec2(100, 26))) {
        g_CurrentUIMode = SELECT_UI;
    }

    ImGui::SetCursorPos(ImVec2(10, 42));
    ImGui::Separator();

    // Left Panel: Instances List
    ImGui::SetCursorPos(ImVec2(10, 52));
    ImGui::BeginChild("InstanceList", ImVec2(280, ImGui::GetIO().DisplaySize.y - 65), true);
    
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.9f, 1.0f), "ACTIVE ROBLOX WINDOWS");
    
    if (ImGui::Button("Refresh List", ImVec2(-1, 32))) {
        RefreshRobloxInstances();
    }
    
    if (ImGui::Button("+ Launch New Roblox", ImVec2(-1, 32))) {
        ShellExecuteA(NULL, "open", "roblox://", NULL, NULL, SW_SHOWNORMAL);
        // รอดาวน์โหลดและเปิดเกม 4 วินาทีแล้ว Refresh อัตโนมัติ
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(4));
            RefreshRobloxInstances();
        }).detach();
    }

    if (ImGui::Button("+ Add Virtual Test Instance", ImVec2(-1, 28))) {
        RobloxInstance inst;
        inst.id = (int)g_Instances.size() + 1;
        inst.pid = 999000 + inst.id;
        inst.accountName = "Virtual Account #" + std::to_string(inst.id);
        inst.hwnd = NULL;
        inst.isConnected = true;
        inst.antiAFK = true;
        inst.lowResourceMode = false;
        inst.fpsCap = 60;
        inst.currentStatus = "Simulated Active";
        g_Instances.push_back(inst);
        if (g_SelectedInstanceIndex == -1) g_SelectedInstanceIndex = 0;
    }

    ImGui::Separator();

    if (g_Instances.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No Roblox window found.\nClick Launch or Add Virtual!");
    } else {
        for (int i = 0; i < (int)g_Instances.size(); i++) {
            std::string label = "  " + g_Instances[i].accountName + "##" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), g_SelectedInstanceIndex == i, 0, ImVec2(0, 32))) {
                g_SelectedInstanceIndex = i;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel: Specific Instance Detailed Control
    ImGui::BeginChild("InstanceDetails", ImVec2(0, ImGui::GetIO().DisplaySize.y - 65), true);
    
    if (g_SelectedInstanceIndex >= 0 && g_SelectedInstanceIndex < (int)g_Instances.size()) {
        auto& inst = g_Instances[g_SelectedInstanceIndex];

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CONTROLLING: %s", inst.accountName.c_str());
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Status: %s", inst.currentStatus.c_str());
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::Checkbox(" Enable Anti-AFK (Bypass 20-min Disconnect)", &inst.antiAFK);
        ImGui::Checkbox(" Low Resource Saver Mode (Background Frame Reduction)", &inst.lowResourceMode);
        
        ImGui::Spacing();
        ImGui::SliderInt(" Target FPS Cap", &inst.fpsCap, 15, 240);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("WINDOW ACTIONS:");
        
        if (ImGui::Button("Bring Window To Front", ImVec2(200, 36))) {
            if (inst.hwnd) {
                ShowWindow(inst.hwnd, SW_RESTORE);
                SetForegroundWindow(inst.hwnd);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Close This Instance", ImVec2(200, 36))) {
            if (inst.hwnd) PostMessage(inst.hwnd, WM_CLOSE, 0, 0);
            else if (inst.pid > 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, inst.pid);
                if (hProc) { TerminateProcess(hProc, 0); CloseHandle(hProc); }
            }
            g_Instances.erase(g_Instances.begin() + g_SelectedInstanceIndex);
            g_SelectedInstanceIndex = g_Instances.empty() ? -1 : 0;
            ImGui::EndChild();
            ImGui::End();
            return;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("POTASSIUM SCRIPT INJECTOR (THIS SCREEN ONLY):");
        static char scriptBuffer[2048] = "print('Hello from Chxl Multi-Manager!')\n-- Auto farm script here...";
        ImGui::InputTextMultiline("##single_script", scriptBuffer, IM_ARRAYSIZE(scriptBuffer), ImVec2(-1, 140));

        if (ImGui::Button("Execute Script to Selected Instance Pipe", ImVec2(-1, 40))) {
            SendScriptToPipe("\\\\.\\pipe\\PotassiumPipe", scriptBuffer);
        }
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "Welcome to CHXL Ultra Multi-Manager!");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Select an active Roblox Instance from the left panel, or click '+ Add Virtual Test Instance' to test controls.");
    }
    ImGui::EndChild();

    ImGui::End();
}

void RenderClassicUI(HWND hwnd) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Classic Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    RenderTopBar(hwnd);

    ImGui::SetCursorPos(ImVec2(15, 10));
    ImGui::Text("CHXL LAUNCHER - CLASSIC EASY MODE");
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 200);
    if (ImGui::Button("Switch UI", ImVec2(100, 26))) g_CurrentUIMode = SELECT_UI;
    
    ImGui::SetCursorPos(ImVec2(10, 42));
    ImGui::Separator();

    ImGui::SetCursorPos(ImVec2(20, 60));
    if (ImGui::Button("Launch New Roblox จอ", ImVec2(220, 45))) {
        ShellExecuteA(NULL, "open", "roblox://", NULL, NULL, SW_SHOWNORMAL);
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(4));
            RefreshRobloxInstances();
        }).detach();
    }
    
    ImGui::SetCursorPos(ImVec2(20, 120));
    ImGui::Text("Running Roblox Accounts Count: %d", (int)g_Instances.size());
    
    ImGui::End();
}

// Main Window Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ChxlUltraClass", NULL };
    ::RegisterClassExW(&wc);
    
    // WS_THICKFRAME สำหรับให้ผู้ใช้ขยายหน้าต่างได้อิสระ
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Chxl Launcher Ultra", WS_POPUP | WS_THICKFRAME | WS_VISIBLE, 100, 100, 1000, 620, NULL, NULL, wc.hInstance, NULL);

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

        // Render Active UI Mode
        switch (g_CurrentUIMode) {
            case LOADING:          RenderLoadingScreen(hwnd); break;
            case SELECT_UI:        RenderUISelectionScreen(hwnd); break;
            case NEO_MODERN_UI:    RenderNeoModernUI(hwnd); break;
            case CLASSIC_UI:       RenderClassicUI(hwnd); break;
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

// Native Resizing & Borderless Control Proc
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int border_width = 8;

        if (pt.y < border_width) {
            if (pt.x < border_width) return HTTOPLEFT;
            if (pt.x > rc.right - border_width) return HTTOPRIGHT;
            return HTTOP;
        }
        if (pt.y > rc.bottom - border_width) {
            if (pt.x < border_width) return HTBOTTOMLEFT;
            if (pt.x > rc.right - border_width) return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }
        if (pt.x < border_width) return HTLEFT;
        if (pt.x > rc.right - border_width) return HTRIGHT;
        break;
    }
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
