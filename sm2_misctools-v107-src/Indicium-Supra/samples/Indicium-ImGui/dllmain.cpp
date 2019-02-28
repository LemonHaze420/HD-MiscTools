/*
MIT License

Copyright (c) 2018 Benjamin Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
//#define RELEASE

#include "dllmain.h"
#include "addrs.h"
#include "structs.h"
#include "func_defs.h"

#include "VersionManager.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define GLOBAL_HOTKEYS f9,f10,f11
#define GLOBAL_HOTKEYS_NUM 3

#define GetKey(x) (key.VirtualKey == x && !key.pressedBefore&& key.pressedNow)

struct KeyPress {
	unsigned char VirtualKey = '\0';

	bool pressedNow;
	bool pressedBefore;
}GLOBAL_HOTKEYS;

// 
// MinHook
// 
#include <MinHook.h>

// 
// STL
// 
#include <mutex>

// 
// POCO
// 
#include <Poco/Message.h>
#include <Poco/Logger.h>
#include <Poco/FileChannel.h>
#include <Poco/AutoPtr.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/Path.h>

using Poco::Message;
using Poco::Logger;
using Poco::FileChannel;
using Poco::AutoPtr;
using Poco::PatternFormatter;
using Poco::FormattingChannel;
using Poco::Path;

// 
// ImGui includes
// 
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_dx10.h>
#include <imgui_impl_dx11.h>

t_WindowProc OriginalDefWindowProc = nullptr;
t_WindowProc OriginalWindowProc = nullptr;
PINDICIUM_ENGINE engine = nullptr;

int	gameID = 0;

static bool ingame_status			= false;
static bool force_60fps				= false;
static bool force_60fps2			= false;
static bool force_timemultiplier	= false;
static bool force_camlock			= false;
static bool enable_freecam			= false;
static bool bShowNoneInTaskView		= false;
static bool bShowActiveInTaskView   = false;
static bool bUnsupported			= false;

static bool has_initialized			= false;

static bool toggle_fov				= false;

vec3 campos;
vec3 playerpos;
vec3 playerpos2;

static float temp_campos[]{ 0,0,0 };
static float temp_playerpos[]{ 0,0,0 };
static float temp_user_cameradistance[]{ 0,0 };

bool has_values_initialized = false;

float timeMultiplier = 1.0f;
float fFrameTime;
float fFPS;

float temp_cameradistance = 3.099999905f;
float temp_cameradistance2 = 3.099999905f;

float cameradistance = 0.0f;

static float temp_fieldofview = 0.0f, fieldofview = 0.0f;

int player_money = 0;
int sega_coins = 0;

int temp_player_money = 0;
int temp_sega_coins = 0;

int temp_camlock = 0;

int user_camerastate = 0;
int temp_camerastate = 0;
int user_freezetime = 0;
int temp_freezetime = 0;

BYTE day, month, year, hour, minute, seconds;

bool force_freezetime = false;
bool toggle_camerastate = false;

bool first_coords = true;

bool show_tasks_in_console = false;
bool show_logger_in_console = false;

bool enable_npc_logger = false;
bool npc_logger_initialized = false;
uint16_t npc_logger_fix = SHENMUE2_V107_ENABLE_NPC_LOGGER_FIX_INS;
char fps_fix[]			= { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
char sm1_60fps_fix[]	= { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

TaskQueue g_TaskQueue;
sm2_ctrl* g_MapControl;

char buffer[256] = "";

static float temp_charPos[]{ 0,0,0 };

std::vector<KeyPress> keyPresses;

char* moneyBuf = { '\0' };

DWORD_PTR gPtr = NULL;

DWORD_PTR GetProcessBaseAddress(DWORD processID)
{
	DWORD_PTR   baseAddress = 0;
	HANDLE      processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
	HMODULE     *moduleArray;
	LPBYTE      moduleArrayBytes;
	DWORD       bytesRequired;

	if (processHandle)
	{
		if (EnumProcessModules(processHandle, NULL, 0, &bytesRequired))
		{
			if (bytesRequired)
			{
				moduleArrayBytes = (LPBYTE)LocalAlloc(LPTR, bytesRequired);

				if (moduleArrayBytes)
				{
					unsigned int moduleCount;

					moduleCount = bytesRequired / sizeof(HMODULE);
					moduleArray = (HMODULE *)moduleArrayBytes;

					if (EnumProcessModules(processHandle, moduleArray, bytesRequired, &bytesRequired))
					{
						baseAddress = (DWORD_PTR)moduleArray[0];
					}
					LocalFree(moduleArrayBytes);
				}
			}
		}

		CloseHandle(processHandle);
	}
	return baseAddress;
}

bool WriteProcessMemoryWrapper(DWORD address, void* buffer, int length)
{
	DWORD_PTR	baseAddress = GetProcessBaseAddress(GetCurrentProcessId());
	HANDLE      processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
	SIZE_T		bytesWritten;
	return WriteProcessMemory(processHandle, (LPVOID)(baseAddress + address), buffer, length, &bytesWritten);
}


//----------------------------------------------------------------------------------------------------------
// Shenmue 1 Cutscene Hooks

struct ns1 {
	signed char pad188[188];
	int32_t f188;
};
struct sm1_audio_struct {
	uint32_t* f0;
	uint32_t f4;
	struct ns1* f8;
	signed char pad24[8];
	unsigned char f24;
};
struct subtitleStruct {
	unsigned char f0;
	char pad5433342948[5433342947];
	signed char f5433342948;
};

typedef void(__fastcall *ProcessSubtitleFile_SM1_t)(char* rcx, int32_t edx, int32_t r8d, struct sm1_audio_struct* r9);
ProcessSubtitleFile_SM1_t ProcessSubtitleFile_SM1_Orig;
typedef void(__fastcall *ProcessSubtitleText_SM1_t)(char* rcx);
ProcessSubtitleText_SM1_t ProcessSubtitleText_SM1_Orig;

char* timeBuffer = { '\0' };

// Shows active subtitle audio filename
void ProcessSubtitleFile_SM1_Hook(char* rcx, int32_t edx, int32_t r8d, struct sm1_audio_struct* r9) {
	printf("[SM1|Audio|%s]: [FILE] %s\n", timeBuffer, rcx);

	ProcessSubtitleFile_SM1_Orig(rcx, edx, r8d, r9);
}

// Shows active subtitle text [copy at 133CAFC]
void ProcessSubtitleText_SM1_Hook(char* rcx) {
	printf("[SM1|Audio|%s]: [SUBTITLE] %s\n", timeBuffer, rcx);

	ProcessSubtitleText_SM1_Orig(rcx);
}
//----------------------------------------------------------------------------------------------------------

// Shenmue 2 Cutscene Hooks...
// Subtitle Audio Filename..
struct subtitleFilenameStruct {
	signed char pad11[11];
	unsigned char f11;
};

typedef void(__fastcall *ProcessSubtitleFile_SM2_t)(int64_t rcx, struct subtitleFilenameStruct * rdx);
ProcessSubtitleFile_SM2_t ProcessSubtitleFile_SM2_Orig;
typedef void(__fastcall *ProcessSubtitleText_SM2_t)(struct subtitleStruct* rcx);
ProcessSubtitleText_SM2_t ProcessSubtitleText_SM2_Orig;

// Sows active subtitle text
void ProcessSubtitleText_SM2_Hook(struct subtitleStruct* rcx) {
	printf("[SM2|Audio|%s]: [SUBTITLE] %c%s\n", timeBuffer, rcx->f0, rcx->pad5433342948);

	ProcessSubtitleText_SM2_Orig(rcx);
}
// Shows active subtitle audio filename
void ProcessSubtitleFile_SM2_Hook(int64_t rcx, struct subtitleFilenameStruct* rdx) {
	printf("[SM2|Audio|%s]: [FILE] %s\n", timeBuffer, rdx->pad11);

	ProcessSubtitleFile_SM2_Orig(rcx, rdx);
}

//----------------------------------------------------------------------------------------------------------
char currentScene[4] = { '\0' };

void EnableD3TDebug()
{
	DWORD_PTR base = GetProcessBaseAddress(GetCurrentProcessId());
	DWORD d3t_1 = 1;
	byte d3tb_1 = 1;
	memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_1), &d3t_1, sizeof(d3t_1));
	memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_2), &d3t_1, sizeof(d3t_1));
	memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_3), &d3t_1, sizeof(d3t_1));

	memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_4), &d3t_1, sizeof(d3t_1));
	//memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_5), &d3t_1, sizeof(d3t_1)); //spams alot
	//memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_6), &d3tb_1, sizeof(d3tb_1));
	//memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_7), &d3tb_1, sizeof(d3tb_1));
	//memcpy((void*)(base + SHENMUE2_V107_ENABLE_D3T_DEBUG_8), &d3tb_1, sizeof(d3tb_1));
}

/**
 * \fn  BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
 *
 * \brief   DLL main entry point. Only Indicium engine initialization or shutdown should happen
 *          here to avoid deadlocks.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    16.06.2018
 *
 * \param   hInstance   The instance handle.
 * \param   dwReason    The call reason.
 * \param   parameter3  Unused.
 *
 * \return  TRUE on success, FALSE otherwise (will abort loading the library).
 */
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
    //
    // We don't need to get notified in thread attach- or detachments
    // 
    DisableThreadLibraryCalls(static_cast<HMODULE>(hInstance));
	
	if (strstr(VersionManager::singleton()->getGameId(), "sm1")) {
		if (VersionManager::singleton()->getVersion() != Version::Coconut107)
			bUnsupported = true;
	}
	else if (strstr(VersionManager::singleton()->getGameId(), "sm2")) {
		if (VersionManager::singleton()->getVersion() != Version::Mango107)
			bUnsupported = true;
		else
			EnableD3TDebug();
	}
	else bUnsupported = true;

	if (bUnsupported) {
		MessageBoxA(NULL, "Unsupported version!", "", MB_OK);
		return FALSE;
	}

    INDICIUM_D3D9_EVENT_CALLBACKS d3d9;
    INDICIUM_D3D9_EVENT_CALLBACKS_INIT(&d3d9);
    d3d9.EvtIndiciumD3D9PrePresent = EvtIndiciumD3D9Present;
    d3d9.EvtIndiciumD3D9PreReset = EvtIndiciumD3D9Reset;
    d3d9.EvtIndiciumD3D9PrePresentEx = EvtIndiciumD3D9PresentEx;
    d3d9.EvtIndiciumD3D9PreResetEx = EvtIndiciumD3D9ResetEx;

    INDICIUM_D3D10_EVENT_CALLBACKS d3d10;
    INDICIUM_D3D10_EVENT_CALLBACKS_INIT(&d3d10);
    d3d10.EvtIndiciumD3D10PrePresent = EvtIndiciumD3D10Present;
    d3d10.EvtIndiciumD3D10PreResizeTarget = EvtIndiciumD3D10ResizeTarget;

    INDICIUM_D3D11_EVENT_CALLBACKS d3d11;
    INDICIUM_D3D11_EVENT_CALLBACKS_INIT(&d3d11);
    d3d11.EvtIndiciumD3D11PrePresent = EvtIndiciumD3D11Present;
    d3d11.EvtIndiciumD3D11PreResizeTarget = EvtIndiciumD3D11ResizeTarget;

    INDICIUM_ERROR err;

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!engine)
        {
            //
            // Get engine handle
            // 
            engine = IndiciumEngineAlloc();

            //
            // Register render pipeline callbacks
            // 
            IndiciumEngineSetD3D9EventCallbacks(engine, &d3d9);
            IndiciumEngineSetD3D10EventCallbacks(engine, &d3d10);
            IndiciumEngineSetD3D11EventCallbacks(engine, &d3d11);

            err = IndiciumEngineInit(engine, EvtIndiciumGameHooked);
        }

        break;
    case DLL_PROCESS_DETACH:

        if (engine)
        {
            IndiciumEngineShutdown(engine, EvtIndiciumGameUnhooked);
            IndiciumEngineFree(engine);
        }

        break;
    default:
        break;
    }

    return TRUE;
}

/**
 * \fn  void EvtIndiciumGameHooked(const INDICIUM_D3D_VERSION GameVersion)
 *
 * \brief   Gets called when the games' rendering pipeline has successfully been hooked and the
 *          rendering callbacks are about to get fired. The detected version of the used
 *          rendering objects is reported as well.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    16.06.2018
 *
 * \param   GameVersion The detected DirectX/Direct3D version.
 */
void EvtIndiciumGameHooked(const INDICIUM_D3D_VERSION GameVersion)
{
    std::string logfile("%TEMP%\\Indicium-ImGui.Plugin.log");

    AutoPtr<FileChannel> pFileChannel(new FileChannel);
    pFileChannel->setProperty("path", Poco::Path::expand(logfile));
    AutoPtr<PatternFormatter> pPF(new PatternFormatter);
    pPF->setProperty("pattern", "%Y-%m-%d %H:%M:%S.%i %s [%p]: %t");
    AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF, pFileChannel));

    Logger::root().setChannel(pFC);

    auto& logger = Logger::get(__func__);

    logger.information("Loading ImGui plugin");

    logger.information("Initializing hook engine...");

    MH_STATUS status = MH_Initialize();

    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        logger.fatal("Couldn't initialize hook engine: %lu", (ULONG)status);
    }

    logger.information("Hook engine initialized");

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup style
    ImGui::StyleColorsDark();
}

/**
 * \fn  void EvtIndiciumGameUnhooked()
 *
 * \brief   Gets called when all core engine hooks have been released. At this stage it is save
 *          to remove our own additional hooks and shut down the hooking sub-system as well.
 *
 * \author  Benjamin "Nefarius" Höglinger
 * \date    16.06.2018
 */
void EvtIndiciumGameUnhooked()
{
    auto& logger = Logger::get(__func__);

    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
    {
        logger.fatal("Couldn't disable hooks, host process might crash");
        return;
    }

    logger.information("Hooks disabled");

    if (MH_Uninitialize() != MH_OK)
    {
        logger.fatal("Couldn't shut down hook engine, host process might crash");
        return;
    }
}

#pragma region D3D9(Ex)

void EvtIndiciumD3D9Present(
    LPDIRECT3DDEVICE9   pDevice,
    const RECT          *pSourceRect,
    const RECT          *pDestRect,
    HWND                hDestWindowOverride,
    const RGNDATA       *pDirtyRegion
)
{
    static auto& logger = Logger::get(__func__);
    static auto initialized = false;
    static std::once_flag init;

    std::call_once(init, [&](LPDIRECT3DDEVICE9 pd3dDevice)
    {
        D3DDEVICE_CREATION_PARAMETERS params;

        auto hr = pd3dDevice->GetCreationParameters(&params);
        if (FAILED(hr))
        {
            logger.error("Couldn't get creation parameters from device");
            return;
        }

        ImGui_ImplDX9_Init(params.hFocusWindow, pd3dDevice);

        logger.information("ImGui (DX9) initialized");

        HookWindowProc(params.hFocusWindow);

        initialized = true;

    }, pDevice);

    if (initialized)
    {
        ImGui_ImplDX9_NewFrame();
        RenderScene();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
}

void EvtIndiciumD3D9Reset(
    LPDIRECT3DDEVICE9       pDevice,
    D3DPRESENT_PARAMETERS   *pPresentationParameters
)
{
    //
    // TODO: more checks and testing!
    // 
    ImGui_ImplDX9_InvalidateDeviceObjects();
    ImGui_ImplDX9_CreateDeviceObjects();
}

void EvtIndiciumD3D9PresentEx(
    LPDIRECT3DDEVICE9EX     pDevice,
    const RECT              *pSourceRect,
    const RECT              *pDestRect,
    HWND                    hDestWindowOverride,
    const RGNDATA           *pDirtyRegion,
    DWORD                   dwFlags
)
{
    static auto& logger = Logger::get(__func__);
    static auto initialized = false;
    static std::once_flag init;

    std::call_once(init, [&](LPDIRECT3DDEVICE9EX pd3dDevice)
    {
        D3DDEVICE_CREATION_PARAMETERS params;

        auto hr = pd3dDevice->GetCreationParameters(&params);
        if (FAILED(hr))
        {
            logger.error("Couldn't get creation parameters from device");
            return;
        }

        ImGui_ImplDX9_Init(params.hFocusWindow, pd3dDevice);

        logger.information("ImGui (DX9Ex) initialized");

        HookWindowProc(params.hFocusWindow);

        initialized = true;

    }, pDevice);

    if (initialized)
    {
        ImGui_ImplDX9_NewFrame();
        RenderScene();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
}

void EvtIndiciumD3D9ResetEx(
    LPDIRECT3DDEVICE9EX     pDevice,
    D3DPRESENT_PARAMETERS   *pPresentationParameters,
    D3DDISPLAYMODEEX        *pFullscreenDisplayMode
)
{
    //
    // TODO: more checks and testing!
    // 
    ImGui_ImplDX9_InvalidateDeviceObjects();
    ImGui_ImplDX9_CreateDeviceObjects();
}

#pragma endregion

#pragma region D3D10

void EvtIndiciumD3D10Present(
    IDXGISwapChain  *pSwapChain,
    UINT            SyncInterval,
    UINT            Flags
)
{
    static auto& logger = Logger::get(__func__);
    static auto initialized = false;
    static std::once_flag init;

    std::call_once(init, [&](IDXGISwapChain *pChain)
    {
        logger.information("Grabbing device and context pointers");

        ID3D10Device *pDevice;
        if (FAILED(D3D10_DEVICE_FROM_SWAPCHAIN(pChain, &pDevice)))
        {
            logger.error("Couldn't get device from swapchain");
            return;
        }

        DXGI_SWAP_CHAIN_DESC sd;
        pChain->GetDesc(&sd);

        logger.information("Initializing ImGui");

        ImGui_ImplDX10_Init(sd.OutputWindow, pDevice);

        logger.information("ImGui (DX10) initialized");

        HookWindowProc(sd.OutputWindow);

        initialized = true;

    }, pSwapChain);

    if (initialized)
    {
        ImGui_ImplDX10_NewFrame();
        RenderScene();
        ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
    }
}

void EvtIndiciumD3D10ResizeTarget(
    IDXGISwapChain          *pSwapChain,
    const DXGI_MODE_DESC    *pNewTargetParameters
)
{
    //
    // TODO: more checks and testing!
    // 
    ImGui_ImplDX10_InvalidateDeviceObjects();
    ImGui_ImplDX10_CreateDeviceObjects();
}

#pragma endregion

#pragma region D3D11

void EvtIndiciumD3D11Present(
    IDXGISwapChain  *pSwapChain,
    UINT            SyncInterval,
    UINT            Flags
)
{
    static auto& logger = Logger::get(__func__);
    static auto initialized = false;
    static std::once_flag init;

    static ID3D11DeviceContext *pContext;
    static ID3D11RenderTargetView *mainRenderTargetView;

    std::call_once(init, [&](IDXGISwapChain *pChain)
    {
        logger.information("Grabbing device and context pointers");

        ID3D11Device *pDevice;
        if (FAILED(D3D11_DEVICE_CONTEXT_FROM_SWAPCHAIN(pChain, &pDevice, &pContext)))
        {
            logger.error("Couldn't get device and context from swapchain");
            return;
        }

        ID3D11Texture2D* pBackBuffer;
        pChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
        pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        pBackBuffer->Release();

        DXGI_SWAP_CHAIN_DESC sd;
        pChain->GetDesc(&sd);

        logger.information("Initializing ImGui");

        ImGui_ImplDX11_Init(sd.OutputWindow, pDevice, pContext);

        logger.information("ImGui (DX11) initialized");

        HookWindowProc(sd.OutputWindow);

        initialized = true;

    }, pSwapChain);

    if (initialized)
    {
        ImGui_ImplDX11_NewFrame();
        pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
        RenderScene();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

void EvtIndiciumD3D11ResizeTarget(
    IDXGISwapChain          *pSwapChain,
    const DXGI_MODE_DESC    *pNewTargetParameters
)
{
    //
    // TODO: more checks and testing!
    // 
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();
}

#pragma endregion

#pragma region WNDPROC Hooking

void HookWindowProc(HWND hWnd)
{
    auto& logger = Logger::get(__func__);

    MH_STATUS ret;

    if ((ret = MH_CreateHook(
        &DefWindowProcW,
        &DetourDefWindowProc,
        reinterpret_cast<LPVOID*>(&OriginalDefWindowProc))
        ) != MH_OK)
    {
        logger.error("Couldn't create hook for DefWindowProcW: %lu", static_cast<ULONG>(ret));
        return;
    }

    if (ret == MH_OK && MH_EnableHook(&DefWindowProcW) != MH_OK)
    {
        logger.error("Couldn't enable DefWindowProcW hook");
    }   

    if ((ret = MH_CreateHook(
        &DefWindowProcA,
        &DetourDefWindowProc,
        reinterpret_cast<LPVOID*>(&OriginalDefWindowProc))
        ) != MH_OK)
    {
        logger.error("Couldn't create hook for DefWindowProcA: %lu", static_cast<ULONG>(ret));
        return;
    }

    if (ret == MH_OK && MH_EnableHook(&DefWindowProcA) != MH_OK)
    {
        logger.error("Couldn't enable DefWindowProcW hook");
    }

    auto lptrWndProc = reinterpret_cast<t_WindowProc>(GetWindowLongPtr(hWnd, GWLP_WNDPROC));

    if (MH_CreateHook(lptrWndProc, &DetourWindowProc, reinterpret_cast<LPVOID*>(&OriginalWindowProc)) != MH_OK)
    {
        logger.warning("Couldn't create hook for GWLP_WNDPROC");
        return;
    }

    if (MH_EnableHook(lptrWndProc) != MH_OK)
    {
        logger.error("Couldn't enable GWLP_WNDPROC hook");
    }
}

LRESULT WINAPI DetourDefWindowProc(
    _In_ HWND hWnd,
    _In_ UINT Msg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    static std::once_flag flag;
    std::call_once(flag, []() {Logger::get("DetourDefWindowProc").information("++ DetourDefWindowProc called"); });

    ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam);

    return OriginalDefWindowProc(hWnd, Msg, wParam, lParam);
}

LRESULT WINAPI DetourWindowProc(
    _In_ HWND hWnd,
    _In_ UINT Msg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    static std::once_flag flag;
    std::call_once(flag, []() {Logger::get("DetourWindowProc").information("++ DetourWindowProc called"); });

    ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam);

    return OriginalWindowProc(hWnd, Msg, wParam, lParam);
}

#pragma endregion

#pragma region Main content rendering

bool bSkipPPFX = true;

DWORD_PTR baseAddr = NULL;

bool bDrawDebugBuffers = false;
void* debugBufferPatch = nullptr;

Hooked_DbgPrint_t iHooked_DbgPrint;
hooked_sub_1404B5030_t orig_sub_1404B5030;
hooked_sub_1404B62C0_t orig_sub_1404B62C0;

DWORD_PTR loggerAddr	 = 0x0;
DWORD_PTR origLoggerAddr = 0x0;

DWORD_PTR otherloggerAddr = 0x0;
DWORD_PTR otherorigLoggerAddr = 0x0;


void Hooked_DbgPrint(char * msg, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, msg);
	vsprintf(buffer, msg, args);
	iHooked_DbgPrint(msg, args);
	va_end(args);

	printf(buffer);

	if (!strstr(buffer, "\n")) {
		printf("\n");
	}

	return;
}

#ifndef RELEASE

SwitchCameraMode_t			SwitchCameraMode;

NPC_Cleanup_Func_1_t		sub_1404796D0;
GetTaskParameterPointer_t	GetTaskParameterPointer;

npc_Callback_Enqueue_t		npc_Callback_Enqueue;
EnqueueTaskWithoutParam_t	EnqueueTaskWithoutParamCall;
EnqueueTaskWithParam_t		EnqueueTaskWithParamCall;
CleanupTask_t				CleanupTaskCall;
CleanupTaskFlags_t			CleanupTaskFlagsCall;

// Function Hooks
EnqueueTaskWithoutParam_t	origEnqueueTaskWithoutParam;
EnqueueTaskWithParam_t		origEnqueueTaskWithParam;
CleanupTask_t				origCleanupTask;
CleanupTaskFlags_t			origCleanupTaskFlags;

__int64 EnqueueTaskWithoutParam(__int64 callbackFunction, uint8_t nextFunctionIndex, uint8_t r8b, int taskToken);
__int64 EnqueueTaskWithParam(__int64 callbackFunction, uint8_t nextFunctionIndex, __int64 a3, __int64 param, int taskToken, char* taskName);

__int64 CleanupTask(__int64 taskPointer);
__int64 CleanupTaskFlags(__int64 taskPointer);

charDispCheck_t origCharDispCheck;
MainLoop_t origMainLoop;

signed __int64 charDispCheck(){
	return origCharDispCheck();
}

static bool bWaiting = false;
static __int64 toDeleteTask = 0;
static Task* taskCallOrder[300];
static int taskCallOrderCounter = 0;

int GetTaskCallIndex(int index) {
	Task* taskList = (Task*)(baseAddr + SHENMUE2_V107_FIRST_TASK);
	taskList = taskList + index;
	for (int i = 0; i < 300; ++i) {
		if (taskList == taskCallOrder[i]) {
			return i;
		}
	}
}

__int64 MainLoop() {
	if (toDeleteTask)
	{
		origCleanupTask(toDeleteTask); 
		toDeleteTask = 0;
	}

	if (baseAddr != NULL) {
		DWORD_PTR currentTaskPtr = *(DWORD_PTR*)(baseAddr + SHENMUE2_V107_CURRENT_TASK);
		Task* currentTask = (Task*)currentTaskPtr;
		if (strncmp(currentTask->taskName, "ROOT", 4) == 0) {
			taskCallOrderCounter = 0;
		} else {
			if (taskCallOrderCounter < 300) {
				taskCallOrder[taskCallOrderCounter] = currentTask;
				taskCallOrderCounter++;
			}
		}
	}

	return (bWaiting?0:origMainLoop());
}
static bool bDrawConsole = true;

struct appConsole
{
	char                  InputBuf[256];
	ImVector<char*>       Items;
	ImVector<char*>       History;
	int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
	ImVector<const char*> Commands;

	appConsole()
	{
		ClearLog();
		memset(InputBuf, 0, sizeof(InputBuf));
		HistoryPos = -1;
		Commands.push_back("HELP");
		Commands.push_back("HISTORY");
		Commands.push_back("CLEAR");
		Commands.push_back("CLASSIFY");  // "classify" is only here to provide an example of "C"+[tab] completing to "CL" and displaying matches.
		Commands.push_back("TOGGLESHOWNONE");
	}
	~appConsole()
	{
		ClearLog();
		for (int i = 0; i < History.Size; i++)
			free(History[i]);
	}

	// Portable helpers
	static int   Stricmp(const char* str1, const char* str2) { int d; while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; } return d; }
	static int   Strnicmp(const char* str1, const char* str2, int n) { int d = 0; while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; n--; } return d; }
	static char* Strdup(const char *str) { size_t len = strlen(str) + 1; void* buff = malloc(len); return (char*)memcpy(buff, (const void*)str, len); }
	static void  Strtrim(char* str) { char* str_end = str + strlen(str); while (str_end > str && str_end[-1] == ' ') str_end--; *str_end = 0; }

	void    ClearLog()
	{
		for (int i = 0; i < Items.Size; i++)
			free(Items[i]);
		Items.clear();
	}

	void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
	{
		// FIXME-OPT
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
		buf[IM_ARRAYSIZE(buf) - 1] = 0;
		va_end(args);
		Items.push_back(Strdup(buf));
	}

	void    Draw(const char* title, bool* p_open)
	{
		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title, p_open))
		{
			ImGui::End();
			return;
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Close Console"))
				*p_open = false;
			ImGui::EndPopup();
		}

		if (ImGui::SmallButton("Clear")) { ClearLog(); } ImGui::SameLine();
		bool copy_to_clipboard = ImGui::SmallButton("Copy"); ImGui::SameLine();

		ImGui::Separator();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		static ImGuiTextFilter filter;
		filter.Draw("Filter (\"incl,-excl\") (\"WithoutParam\")", 180);
		ImGui::PopStyleVar();
		ImGui::Separator();

		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); // 1 separator, 1 input text
		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar); // Leave room for 1 separator + 1 InputText
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::Selectable("Clear")) ClearLog();
			ImGui::EndPopup();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		if (copy_to_clipboard)
			ImGui::LogToClipboard();
		ImVec4 col_default_text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		for (int i = 0; i < Items.Size; i++)
		{
			const char* item = Items[i];
			if (!filter.PassFilter(item))
				continue;
			ImVec4 col = col_default_text;
			if (strstr(item, "WithoutParam")) col = ImColor(1.0f, 0.4f, 0.4f, 1.0f);
			else if (strncmp(item, "# ", 2) == 0) col = ImColor(1.0f, 0.78f, 0.58f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, col);
			ImGui::TextUnformatted(item);
			ImGui::PopStyleColor();
		}
		if (copy_to_clipboard)
			ImGui::LogFinish();
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::Separator();

		// Command-line
		bool reclaim_focus = false;
		if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory, &TextEditCallbackStub, (void*)this))
		{
			char* s = InputBuf;
			Strtrim(s);
			if (s[0])
				ExecCommand(s);
			strcpy(s, "");
			reclaim_focus = true;
		}

		// Auto-focus on window apparition
		ImGui::SetItemDefaultFocus();
		//if (reclaim_focus)
			//ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

		ImGui::End();
	}

	void    ExecCommand(const char* command_line)
	{
		AddLog("# %s\n", command_line);

		// Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or optimal.
		HistoryPos = -1;
		for (int i = History.Size - 1; i >= 0; i--)
			if (Stricmp(History[i], command_line) == 0)
			{
				free(History[i]);
				History.erase(History.begin() + i);
				break;
			}
		History.push_back(Strdup(command_line));

		// Process command
		if (Stricmp(command_line, "CLEAR") == 0)
		{
			ClearLog();
		}
		else if (Stricmp(command_line, "HELP") == 0)
		{
			AddLog("Commands:");
			for (int i = 0; i < Commands.Size; i++)
				AddLog("- %s", Commands[i]);
		}
		else if (Stricmp(command_line, "HISTORY") == 0)
		{
			int first = History.Size - 10;
			for (int i = first > 0 ? first : 0; i < History.Size; i++)
				AddLog("%3d: %s\n", i, History[i]);
		}
		else if (Stricmp(command_line, "TOGGLESHOWNONE") == 0)
		{
			bShowNoneInTaskView = !bShowNoneInTaskView;
		}
		else
		{
			AddLog("Unknown command: '%s'\n", command_line);
		}
	}

	static int TextEditCallbackStub(ImGuiTextEditCallbackData* data) // In C++11 you are better off using lambdas for this sort of forwarding callbacks
	{
		appConsole* console = (appConsole*)data->UserData;
		return console->TextEditCallback(data);
	}

	int     TextEditCallback(ImGuiTextEditCallbackData* data)
	{
		//AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
		switch (data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
		{
			// Example of TEXT COMPLETION

			// Locate beginning of current word
			const char* word_end = data->Buf + data->CursorPos;
			const char* word_start = word_end;
			while (word_start > data->Buf)
			{
				const char c = word_start[-1];
				if (c == ' ' || c == '\t' || c == ',' || c == ';')
					break;
				word_start--;
			}

			// Build a list of candidates
			ImVector<const char*> candidates;
			for (int i = 0; i < Commands.Size; i++)
				if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
					candidates.push_back(Commands[i]);

			if (candidates.Size == 0)
			{
				// No match
				AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
			}
			else if (candidates.Size == 1)
			{
				// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
				data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
				data->InsertChars(data->CursorPos, candidates[0]);
				data->InsertChars(data->CursorPos, " ");
			}
			else
			{
				// Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display "CLEAR" and "CLASSIFY"
				int match_len = (int)(word_end - word_start);
				for (;;)
				{
					int c = 0;
					bool all_candidates_matches = true;
					for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
						if (i == 0)
							c = toupper(candidates[i][match_len]);
						else if (c == 0 || c != toupper(candidates[i][match_len]))
							all_candidates_matches = false;
					if (!all_candidates_matches)
						break;
					match_len++;
				}

				if (match_len > 0)
				{
					data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
					data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
				}

				// List matches
				AddLog("Possible matches:\n");
				for (int i = 0; i < candidates.Size; i++)
					AddLog("- %s\n", candidates[i]);
			}

			break;
		}
		case ImGuiInputTextFlags_CallbackHistory:
		{
			// Example of HISTORY
			const int prev_history_pos = HistoryPos;
			if (data->EventKey == ImGuiKey_UpArrow)
			{
				if (HistoryPos == -1)
					HistoryPos = History.Size - 1;
				else if (HistoryPos > 0)
					HistoryPos--;
			}
			else if (data->EventKey == ImGuiKey_DownArrow)
			{
				if (HistoryPos != -1)
					if (++HistoryPos >= History.Size)
						HistoryPos = -1;
			}

			// A better implementation would preserve the data on the current input line along with cursor position.
			if (prev_history_pos != HistoryPos)
			{
				const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, history_str);
			}
		}
		}
		return 0;
	}
};

static appConsole console;

__int64 EnqueueTaskWithoutParam(__int64 callbackFunction, uint8_t nextFunctionIndex, uint8_t r8b, int taskToken) {
	__int64 result = origEnqueueTaskWithoutParam(callbackFunction, nextFunctionIndex, r8b, taskToken);
#	ifdef _DEBUG
	if(show_tasks_in_console)
		printf("EnqueueTaskWithoutParam(0x%Ix, 0x%x, 0x%x, \"%c%c%c%c\") returned 0x%Ix\n", callbackFunction, nextFunctionIndex, r8b, taskToken & 0xFF, (taskToken >> 8) & 0xFF, (taskToken >> 16) & 0xFF, (taskToken >> 24) & 0xFF, result);

	console.AddLog("EnqueueTaskWithoutParam(0x%Ix, 0x%x, 0x%x, \"%c%c%c%c\") returned 0x%Ix\n", callbackFunction, nextFunctionIndex, r8b, taskToken & 0xFF, (taskToken >> 8) & 0xFF, (taskToken >> 16) & 0xFF, (taskToken >> 24) & 0xFF, result);
#	endif
	return result;
}
__int64 EnqueueTaskWithParam(__int64 callbackFunction, uint8_t nextFunctionIndex, __int64 a3, __int64 param, int taskToken, char* taskName) {
	__int64 result = origEnqueueTaskWithParam(callbackFunction, nextFunctionIndex, a3, param, taskToken, taskName);
#	ifdef _DEBUG
	if (show_tasks_in_console)
		printf("EnqueueTaskWithParam(0x%Ix, 0x%x, 0x%Ix, 0x%Ix, \"%c%c%c%c\",\"%s\") returned 0x%Ix\n", callbackFunction, nextFunctionIndex, a3, param, taskToken & 0xFF, (taskToken >> 8) & 0xFF, (taskToken >> 16) & 0xFF, (taskToken >> 24) & 0xFF, taskName, result);

	console.AddLog("EnqueueTaskWithParam(0x%Ix, 0x%x, 0x%Ix, 0x%Ix, \"%c%c%c%c\",\"%s\") returned 0x%Ix\n", callbackFunction, nextFunctionIndex, a3, param, taskToken & 0xFF, (taskToken >> 8) &0xFF, (taskToken>>16)&0xFF, (taskToken>>24)&0xFF, taskName, result);
#	endif
	return result;
}
#endif

__int64 CleanupTask(__int64 taskPointer) {
	__int64 result = origCleanupTask(taskPointer);
	return result;
}

__int64 CleanupTaskFlags(__int64 taskPointer) {
	__int64 result = origCleanupTaskFlags(taskPointer);
	return result;
}

__int64 __fastcall HookedLoggerFunc(char* msg, DWORD* a2, char* a3)
{
	if(show_logger_in_console)
		printf("%s: %x %s%s", msg, a2, a3, (!strstr(msg, "\n") ? "\n" : ""));

	return 0i64;
}
__int64 __fastcall OtherHookedLoggerFunc(char* msg)
{
	if (show_logger_in_console)
		printf("%s%s", msg, (!strstr(msg, "\n")?"\n":""));
	return 0i64;
}
void __fastcall hooked_sub_1404B62C0(struct charID* a1, int32_t edx, int32_t r8d, int32_t r9d)
{
	//printf("sub_1404B62C0(\"%c%c%c%c\", 0x%X, 0x%X, \"%c%c%c%c\")\n", a1->ID[0], a1->ID[1] , a1->ID[2], a1->ID[3], edx, r8d, r9d & 0xFF, (r9d >> 8) & 0xFF, (r9d >> 16) & 0xFF, (r9d >> 24) & 0xFF);
	orig_sub_1404B62C0(a1, edx, r8d, r9d);
}
void hex_dump(char *str, unsigned char *buf, int size)
{
	if (str)
		printf("%s:", str);
	for (int i = 0; i<size; ++i) {
		if ((i % 16) == 0) {
			printf("\n%4X:", i);
		}
		printf(" %02X", buf[i]);
	}
	printf("\n\n");
}

char* user_mapID, *mapID;
int sceneID = 1, entryID = 0;
int user_sceneID = 1, user_entryID = 0;

void RenderScene()
{
	static std::once_flag flag;
	std::call_once(flag, []() {
		Logger::get("RenderScene").information("++ RenderScene called");
		
		if (strstr(VersionManager::singleton()->getGameId(), "sm1"))
			gameID = 1;
		else if (strstr(VersionManager::singleton()->getGameId(), "sm2"))
			gameID = 2;
	});

	static bool show_overlay = true;

	if (show_overlay)
	{
		if (gameID == 1)
		{
			ImGui::Begin("Shenmue 1 v1.07 Misc Tools - LemonHaze", nullptr);
			{
				ImGui::Text("F9  : Toggle Tasks Window");
				ImGui::Text("F10 : Toggle Main Window");
				ImGui::Text("F11 : Toggle Patches");
				ImGui::Separator();

				ImGui::Text("Game Date: %02d/%02d/%d\nGame Time: %02d:%02d:%02d\n", day, month, (1900 + year), hour, minute, seconds);
				ImGui::Text("Frame: %0.2fms\nFPS: %0.2f\n", fFrameTime, fFPS);
				ImGui::Separator();

				ImGui::Checkbox("Enable Patches", &ingame_status);	ImGui::SameLine();
				ImGui::Text("\t\tDO NOT ENABLE PATCHES WHILST IN MENU");
				ImGui::Separator();

				if (ingame_status)
				{
					ImGui::Text("Player Statistics");
					ImGui::Separator();

					ImGui::Text("X: %.04f\nY: %.04f\nZ: %.04f\n", playerpos.x, playerpos.y, playerpos.z);
					ImGui::InputFloat3("Warp Position", temp_playerpos, 4);
					if (ImGui::Button("Warp"))
					{
						DWORD_PTR playerStruct = *(DWORD_PTR*)(baseAddr + SHENMUE1_V107_CHAR_STRUCT_PTR);

						if (playerStruct > 0) {
							memcpy((void*)(playerStruct + SHENMUE1_V107_PLAYERPOSX_OFFSET), &temp_playerpos[0], sizeof(float));
							memcpy((void*)(playerStruct + SHENMUE1_V107_PLAYERPOSY_OFFSET), &temp_playerpos[1], sizeof(float));
							memcpy((void*)(playerStruct + SHENMUE1_V107_PLAYERPOSZ_OFFSET), &temp_playerpos[2], sizeof(float));
						}
					}

					ImGui::SliderInt("Money", &player_money, 0, 10000);
					ImGui::SliderInt("SEGA Coins", &sega_coins, 0, 10000);
					ImGui::Separator();
					
					ImGui::Text("Map Warping");
					ImGui::Separator();

					ImGui::InputText("Map ID", buffer, 5);
					ImGui::InputInt("Scene ID", &user_sceneID);
					ImGui::InputInt("Entry ID", &user_entryID);

					if (ImGui::Button("Warp Map"))
					{
						if (sceneID > 0)
						{
							*(BYTE*)(baseAddr + 0x64C89A8) = 7;

							*(BYTE*)(baseAddr + SHENMUE1_V107_SCENE_ID) = (BYTE)user_sceneID;
							*(BYTE*)(baseAddr + SHENMUE1_V107_ENTRY_ID) = (BYTE)user_entryID;

							memcpy((void*)(baseAddr + SHENMUE1_V107_AREA_ID_STRING), (void*)buffer, 4);

							*(BYTE*)(baseAddr + SHENMUE1_V107_TRIGGER_AREA_WARP) = 1;
						}
					}
					ImGui::Separator();

					ImGui::Text("Camera");
					ImGui::Separator();
					ImGui::Text("X: %.04f\nY: %.04f\nZ: %.04f\n", campos.x, campos.y, campos.z);

					ImGui::InputFloat3("Camera Position", temp_campos, 4);
					if (ImGui::Button("Teleport"))
					{
						memcpy((void*)(baseAddr + SHENMUE1_V107_CAMPOSX), &temp_campos[0], sizeof(float));
						memcpy((void*)(baseAddr + SHENMUE1_V107_CAMPOSY), &temp_campos[1], sizeof(float));
						memcpy((void*)(baseAddr + SHENMUE1_V107_CAMPOSZ), &temp_campos[2], sizeof(float));
					}

					ImGui::InputInt("Camera State", &user_camerastate);
					if (ImGui::Checkbox("Force", &force_camlock))
					{
						if (force_camlock) memset((void*)(baseAddr + SHENMUE1_V107_CAMSTATE), user_camerastate, 1);
						else if (!force_camlock) memset((void*)(baseAddr + SHENMUE1_V107_CAMSTATE), 0, 1);
					}

					ImGui::InputFloat("Camera Distance from Player", temp_user_cameradistance, 0.1f);
					ImGui::Separator();

					ImGui::Text("Framerate");
					ImGui::Separator();
					ImGui::Checkbox(">=200 FPS", &force_60fps);
					ImGui::Checkbox(">=60 FPS", &force_60fps2);
					ImGui::Separator();

					ImGui::Text("Time");
					ImGui::Separator();
					ImGui::Checkbox("Freeze Time", &force_freezetime);
					ImGui::Checkbox("Force", &force_timemultiplier);
					ImGui::SliderFloat("Time Multiplier", &timeMultiplier, -10.0f, 10.0f);
					ImGui::Separator();

				}
			}
			ImGui::End();
		}
		else if (gameID == 2)
		{
#		ifndef RELEASE
			console.Draw("Task Console", &bDrawConsole);
#		endif

			ImGui::Begin("Task View");
			{
				/*if (ImGui::Button("Go!"))
				{
					charID cID;

					cID.ID[0] = '_';
					cID.ID[1] = 'B';
					cID.ID[2] = 'Y';
					cID.ID[3] = 'S';

					orig_sub_1404B62C0(&cID, 0x2, 0x5, (int32_t)"pKAP");
				}*/

				ImGui::Checkbox("Show Empty", &bShowNoneInTaskView); ImGui::SameLine();
				ImGui::Checkbox("Show Active", &bShowActiveInTaskView);
				char *taskName = new char[256];
				for (int i = 0; i < 299; ++i) {

					Task& task = g_TaskQueue.Tasks[i];
					if (bShowActiveInTaskView) {
						if (taskCallOrder[i] == NULL) continue;
						sprintf(taskName, "[IDX %d] %.4s", i, taskCallOrder[i]->taskName);
						task = *taskCallOrder[i];
					}
					else {
						sprintf(taskName, "[ID %d] %.4s", i, g_TaskQueue.Tasks[i].taskName);
					}

					if (!bShowNoneInTaskView && strstr(taskName, "NONE"))
						break;

					if (ImGui::TreeNode(taskName)) {

						if (ImGui::Button("Delete")) {
							toDeleteTask = (__int64)(&task);
							//origCleanupTask((__int64)&task);
						}

						ImGui::InputText("Callback Adddress: ", buffer, 256, ImGuiInputTextFlags_CharsHexadecimal);		ImGui::SameLine();
						if (ImGui::Button("Confirm")) {
							uint64_t userCallbackAddr = strtoull(buffer, NULL, 16);
							printf("User: 0x%I64x\nActual: 0x%I64X\n", userCallbackAddr, &task.callbackFuncPtr);
						}
						ImGui::Separator();

						if (ImGui::Button("Dump Task")) {
							hex_dump(taskName, (unsigned char*)&task, 0x74);
						}

						ImGui::Separator();
						ImGui::Text("Callback Address: 0x%I64x\n", task.callbackFuncPtr);  if (ImGui::Button("Dump Callback")) hex_dump(taskName, (unsigned char*)&task.callbackFuncPtr, 0x200);
						ImGui::Separator();
						ImGui::Text("0x0C: 0x%Ix\n\t", task.unk1);
						ImGui::Text("0x0D: 0x%Ix\n\t", task.unk2);
						ImGui::Text("0x10: 0x%Ix\n\t", task.unk3);
						ImGui::Text("0x18: 0x%Ix\n\t", task.nextTask); ImGui::SameLine(); if (ImGui::Button("Dump Next Task")) hex_dump(taskName, (unsigned char*)&task.nextTask, 0x200);
						ImGui::Text("0x20: 0x%Ix\n\t", task.unk5);
						ImGui::Text("0x68: 0x%I64x\n", task.callbackParamPtr); ImGui::SameLine(); if (ImGui::Button("Dump Param")) hex_dump(taskName, (unsigned char*)&task.callbackParamPtr, 0x200);
						ImGui::Separator();

						if (strstr(task.taskName, "CHAR")) {
							if (task.callbackParamPtr) {
								sm2_cwep* cwep = reinterpret_cast<sm2_cwep*>(task.callbackParamPtr);
								ImGui::Text("X: %.04f\nY: %.04f\nZ: %.04f\n", cwep->posX, cwep->posY, cwep->posZ);
								ImGui::InputFloat3("Warp Position", temp_charPos, 4);
								if (ImGui::Button("Warp")) {
									cwep->posX = temp_charPos[0];
									cwep->posY = temp_charPos[1];
									cwep->posZ = temp_charPos[2];
								}
								ImGui::Separator();
							}
						}

						if (strstr(task.taskName, "CTRL")) {
							if (task.callbackParamPtr) {
								sm2_ctrl* ctrl = reinterpret_cast<sm2_ctrl*>(task.callbackParamPtr);
								ImGui::Text("CWEP Address: 0x%I64x\n", ctrl->playerCwep);

								ImGui::InputText("New CWEP Address: ", buffer, 256, ImGuiInputTextFlags_CharsHexadecimal);		ImGui::SameLine();
								if (ImGui::Button("Confirm")) {
									uint64_t userCallbackAddr = strtoull(buffer, NULL, 16);
									printf("User: 0x%I64x\nActual: 0x%I64X\n", userCallbackAddr, ctrl->playerCwep);
									ctrl->playerCwep = (uint64_t*)userCallbackAddr;
								}
								ImGui::Separator();
							}
						}

						if (strstr(task.taskName, "SCEN")) {
							if (task.callbackParamPtr) {
								sm2_scen* scen = reinterpret_cast<sm2_scen*>(task.callbackParamPtr);
								ImGui::Text("Char1: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr1)->charID);
								ImGui::Text("Char2: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr2)->charID);
								ImGui::Text("Char3: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr3)->charID);
								ImGui::Text("Char4: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr4)->charID);
								ImGui::Text("Char5: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr5)->charID);
								ImGui::Text("Char6: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr6)->charID);
								ImGui::Text("Char7: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr7)->charID);
								ImGui::Text("Char8: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr8)->charID);
								ImGui::Text("Char9: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr9)->charID);
								ImGui::Text("Char10: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr10)->charID);
								ImGui::Text("Char11: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr11)->charID);
								ImGui::Text("Char12: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr12)->charID);
								ImGui::Text("Char13: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr13)->charID);
								ImGui::Text("Char14: %s\n", reinterpret_cast<sm2_chid*>(scen->chidPtr14)->charID);
								ImGui::Separator();
							}
						}

						ImGui::TreePop();
					}
				}
				delete[] taskName;
			}
			ImGui::End();

			ImGui::Begin("Shenmue 2 v1.07 Misc Tools - LemonHaze", nullptr);
			{
				ImGui::Text("F9  : Toggle Tasks Window");
				ImGui::Text("F10 : Toggle Main Window");
				ImGui::Text("F11 : Toggle Patches");
				ImGui::Separator();

				ImGui::Text("Game Date: %02d/%02d/%d\nGame Time: %02d:%02d:%02d\n", day, month, (1900 + year), hour, minute, seconds);
				ImGui::Text("Frame: %0.2fms\nFPS: %0.2f\n", fFrameTime, fFPS);

				ImGui::Checkbox("Enable Tasks in Console", &show_tasks_in_console);	ImGui::SameLine();
				ImGui::Checkbox("Enable Logger in Console", &show_logger_in_console);
				ImGui::Checkbox("Enable NPC Logger", &enable_npc_logger); //ImGui::SameLine();
				ImGui::Separator();

				ImGui::Checkbox("Enable Patches", &ingame_status);	ImGui::SameLine();
				ImGui::Text("\t\tDO NOT ENABLE PATCHES WHILST IN MENU");
				ImGui::Separator();

				if (ingame_status)
				{
					ImGui::Text("Player Statistics");
					ImGui::Separator();
					ImGui::Text("X: %.04f\nY: %.04f\nZ: %.04f\n", playerpos.x, playerpos.y, playerpos.z);     ImGui::SameLine(); ImGui::Checkbox("", &first_coords);
					ImGui::Text("X: %.04f\nY: %.04f\nZ: %.04f\n", playerpos2.x, playerpos2.y, playerpos2.z);
					ImGui::InputFloat3("Warp Position", temp_playerpos, 4);

					if (ImGui::Button("Warp"))
					{
						if (first_coords)
						{
							memcpy((void*)(baseAddr + SHENMUE2_V107_PLAYERPOSX), &temp_playerpos[0], sizeof(float));
							memcpy((void*)(baseAddr + SHENMUE2_V107_PLAYERPOSY), &temp_playerpos[1], sizeof(float));
							memcpy((void*)(baseAddr + SHENMUE2_V107_PLAYERPOSZ), &temp_playerpos[2], sizeof(float));
						}
						else
						{
							memcpy((void*)(baseAddr + SHENMUE2_V107_PLAYERPOSX2), &temp_playerpos[0], sizeof(float));
							memcpy((void*)(baseAddr + SHENMUE2_V107_PLAYERPOSY2), &temp_playerpos[1], sizeof(float));
							memcpy((void*)(baseAddr + SHENMUE2_V107_PLAYERPOSZ2), &temp_playerpos[2], sizeof(float));
						}
					}

					ImGui::SliderInt("Money", &player_money, 0, 10000);
					ImGui::SliderInt("SEGA Coins", &sega_coins, 0, 10000);
					ImGui::Separator();

					ImGui::Text("Camera");
					ImGui::Separator();
					ImGui::Text("X: %.04f\nY: %.04f\nZ: %.04f\n", campos.x, campos.y, campos.z);

					ImGui::InputFloat3("Camera Position", temp_campos, 4);
					if (ImGui::Button("Teleport"))
					{
						memcpy((void*)(baseAddr + SHENMUE2_V107_CAMPOSX), &temp_campos[0], sizeof(float));
						memcpy((void*)(baseAddr + SHENMUE2_V107_CAMPOSY), &temp_campos[1], sizeof(float));
						memcpy((void*)(baseAddr + SHENMUE2_V107_CAMPOSZ), &temp_campos[2], sizeof(float));
					}
					if (ImGui::Checkbox("Toggle Camera Lock", &force_camlock))
					{
						if (force_camlock) memset((void*)(baseAddr + SHENMUE2_V107_CAMSTATE), 38, 1);
						else if (!force_camlock) memset((void*)(baseAddr + SHENMUE2_V107_CAMSTATE), 0, 1);
					}

					ImGui::InputFloat2("Camera Distance from Player", temp_user_cameradistance, 9);
					ImGui::Separator();

					ImGui::Text("Framerate");
					ImGui::Separator();
					ImGui::Checkbox(">=200 FPS", &force_60fps);
					ImGui::Checkbox(">=60 FPS", &force_60fps2);
					ImGui::Separator();

					ImGui::Text("Time");
					ImGui::Separator();
					ImGui::Checkbox("Freeze Time", &force_freezetime);
					ImGui::Checkbox("Force", &force_timemultiplier);
					ImGui::SliderFloat("Time Multiplier", &timeMultiplier, -10.0f, 10.0f);
					ImGui::Separator();

				}
			}
			ImGui::End();
		}
    }

	for (auto& key : keyPresses)
	{
		if (GetAsyncKeyState(key.VirtualKey) & 0x8000) key.pressedNow = true; else { key.pressedBefore = false;	key.pressedNow = false; }

		if (GetKey(VK_F9)) {
			bDrawConsole = !bDrawConsole;
			key.pressedBefore = true;
		}
		if (GetKey(VK_F10))		{
			show_overlay = !show_overlay;
			key.pressedBefore = true;
		}
		if (GetKey(VK_F11))		{
			ingame_status = !ingame_status;
			key.pressedBefore = true;
		}
	}

    ImGui::Render();
}

#pragma endregion

#pragma region ImGui-specific (taken from their examples unmodified)

bool ImGui_ImplWin32_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor(NULL);
    }
    else
    {
        // Hardware cursor type
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
        case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
        case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
        case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
        case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
        case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
        case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
        }
        ::SetCursor(::LoadCursor(NULL, win32_cursor));
    }
    return true;
}

// Allow compilation with old Windows SDK. MinGW doesn't have default _WIN32_WINNT/WINVER versions.
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

// Process Win32 mouse/keyboard inputs. 
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
// PS: In this Win32 handler, we use the capture API (GetCapture/SetCapture/ReleaseCapture) to be able to read mouse coordinations when dragging mouse outside of our window bounds.
// PS: We treat DBLCLK messages as regular mouse down messages, so this code will work on windows classes that have the CS_DBLCLKS flag set. Our own example app code doesn't set this flag.
IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() == NULL)
        return 0;

	{
		if (!has_initialized)
		{
			MH_Initialize();
			baseAddr = GetProcessBaseAddress(GetCurrentProcessId());
			AllocConsole();
			freopen("CON", "w", stdout);

			printf("baseAddress: %Ix\n", baseAddr);

			if (gameID == 1)
			{
				DWORD_PTR ProcessSubtitleFile_SM1_Offset = baseAddr + 0x912D0;
				MH_CreateHook(reinterpret_cast<void**>(ProcessSubtitleFile_SM1_Offset), ProcessSubtitleFile_SM1_Hook, reinterpret_cast<void**>(&ProcessSubtitleFile_SM1_Orig));

				MH_STATUS status = MH_EnableHook(reinterpret_cast<void*>(ProcessSubtitleFile_SM1_Offset));
				printf("ProcessSubtitleFile_SM1 returned %d\n", status);

				DWORD_PTR ProcessSubtitleText_SM1_Offset = baseAddr + 0x65180;
				MH_CreateHook(reinterpret_cast<void**>(ProcessSubtitleText_SM1_Offset), ProcessSubtitleText_SM1_Hook, reinterpret_cast<void**>(&ProcessSubtitleText_SM1_Orig));

				status = MH_EnableHook(reinterpret_cast<void*>(ProcessSubtitleText_SM1_Offset));
				printf("ProcessSubtitleText_SM1 returned %d\n", status);

				has_initialized = true;
			}
			else if (gameID == 2)
			{
				
				// Main loop hook: (SM2)	
				EnqueueTaskWithoutParamCall = (EnqueueTaskWithoutParam_t)(baseAddr + SHENMUE2_V107_ENQUEUE_TASK_FUNC);
				EnqueueTaskWithParamCall = (EnqueueTaskWithParam_t)(baseAddr + SHENMUE2_V107_ENQUEUE_TASK_WITH_PARAM_FUNC);
				npc_Callback_Enqueue = (npc_Callback_Enqueue_t)(baseAddr + SHENMUE2_V107_NPC_CALLBACK_ENQUEUE);

				sub_1404796D0 = (NPC_Cleanup_Func_1_t)(baseAddr + 0x4796D0);
				GetTaskParameterPointer = (GetTaskParameterPointer_t)(baseAddr + SHENMUE2_V107_GETTASKPARAMPTR);

				SwitchCameraMode = (SwitchCameraMode_t)(baseAddr + SHENMUE2_V107_SWITCHCAMERAMODE);

				MH_STATUS status;
				DWORD_PTR dbgPrintfHook = baseAddr + SHENMUE2_V107_VSPRINTF_FUNC_3;
				DWORD_PTR mainLoopHook = baseAddr + SHENMUE2_V107_MAINLOOP_FUNC;
				DWORD_PTR charDispCheckHook = baseAddr + SHENMUE2_V107_CHARDISPCHECK;
				DWORD_PTR EnqueueTaskWithoutParamHook = baseAddr + SHENMUE2_V107_ENQUEUE_TASK_FUNC;
				DWORD_PTR EnqueueTaskWithParamHook = baseAddr + SHENMUE2_V107_ENQUEUE_TASK_WITH_PARAM_FUNC;
				DWORD_PTR CleanupTaskHook = baseAddr + SHENMUE2_V107_TASK_CLEANUP_FUNC;
				DWORD_PTR CleanupTaskFlagsHook = baseAddr + SHENMUE2_V107_TASK_CLEANUP_FLAGS_FUNC;
				DWORD_PTR sub_1404B62C0Offset = baseAddr + 0x4B62C0;
				DWORD_PTR ProcessSubtitleFile_SM2_Offset = baseAddr + 0x5CCCB0;
				DWORD_PTR ProcessSubtitleText_SM2_Offset = baseAddr + 0x59AED0;
				
				MH_CreateHook(reinterpret_cast<void*>(sub_1404B62C0Offset), hooked_sub_1404B62C0, reinterpret_cast<void**>(&orig_sub_1404B62C0));
				MH_CreateHook(reinterpret_cast<void*>(dbgPrintfHook), Hooked_DbgPrint, reinterpret_cast<void**>(&iHooked_DbgPrint));
				MH_CreateHook(reinterpret_cast<void*>(mainLoopHook), MainLoop, reinterpret_cast<void**>(&origMainLoop));
				MH_CreateHook(reinterpret_cast<void*>(charDispCheckHook), charDispCheck, reinterpret_cast<void**>(&origCharDispCheck));
				MH_CreateHook(reinterpret_cast<void*>(EnqueueTaskWithParamHook), EnqueueTaskWithParam, reinterpret_cast<void**>(&origEnqueueTaskWithParam));
				MH_CreateHook(reinterpret_cast<void*>(EnqueueTaskWithoutParamHook), EnqueueTaskWithoutParam, reinterpret_cast<void**>(&origEnqueueTaskWithoutParam));
				MH_CreateHook(reinterpret_cast<void*>(CleanupTaskHook), CleanupTask, reinterpret_cast<void**>(&origCleanupTask));
				MH_CreateHook(reinterpret_cast<void*>(CleanupTaskFlagsHook), CleanupTaskFlags, reinterpret_cast<void**>(&origCleanupTaskFlags));
				MH_CreateHook(reinterpret_cast<void**>(ProcessSubtitleText_SM2_Offset), ProcessSubtitleText_SM2_Hook, reinterpret_cast<void**>(&ProcessSubtitleText_SM2_Orig));
				MH_CreateHook(reinterpret_cast<void**>(ProcessSubtitleFile_SM2_Offset), ProcessSubtitleFile_SM2_Hook, reinterpret_cast<void**>(&ProcessSubtitleFile_SM2_Orig));

				status = MH_EnableHook(reinterpret_cast<void*>(mainLoopHook));
				printf("mainLoopHook returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(sub_1404B62C0Offset));
				printf("sub_1404B62C0 returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(dbgPrintfHook));
				printf("dbgPrintfHook returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(charDispCheckHook));
				printf("charDispCheckHook returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(EnqueueTaskWithParamHook));
				printf("EnqueueTaskWithParamHook returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(EnqueueTaskWithoutParamHook));
				printf("EnqueueTaskWithoutParamHook returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(ProcessSubtitleText_SM2_Offset));
				printf("ProcessSubtitleText_SM2_Offset returned %d\n", status);

				status = MH_EnableHook(reinterpret_cast<void*>(ProcessSubtitleFile_SM2_Offset));
				printf("ProcessSubtitleFile_SM2_Offset returned %d\n", status);

				origLoggerAddr = baseAddr + SHENMUE2_V107_LOGGER_FUNC;
				*(DWORD_PTR*)origLoggerAddr = (DWORD_PTR)HookedLoggerFunc;

				otherorigLoggerAddr = baseAddr + 0x8281140;
				*(DWORD_PTR*)otherorigLoggerAddr = (DWORD_PTR)OtherHookedLoggerFunc;

				has_initialized = true;
			}

			// Register key presses once
			f9.VirtualKey = VK_F9;
			f10.VirtualKey = VK_F10;
			f11.VirtualKey = VK_F11;
			keyPresses.push_back(f9); keyPresses.push_back(f10); keyPresses.push_back(f11);
		}

		std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		timeBuffer = std::ctime(&time);

		timeBuffer[strcspn(timeBuffer, "\n")] = 0;

		if (gameID == 1) 
		{
			DWORD_PTR playerStruct = *(DWORD_PTR*)(baseAddr + SHENMUE1_V107_CHAR_STRUCT_PTR);
			
			fFrameTime = *(float*)(baseAddr + SHENMUE1_V107_FRAMETIME)*1000.0f;
			fFPS = (1000.0f / fFrameTime);

			temp_player_money = *(int*)(baseAddr + SHENMUE1_V107_MONEY);
			temp_sega_coins = *(int*)(baseAddr + SHENMUE1_V107_SEGACOINS);
			temp_cameradistance = *(float*)(baseAddr + SHENMUE1_V107_CAMDISTANCE);
			temp_camerastate = *(int*)(baseAddr + SHENMUE1_V107_CAMSTATE);

			day = *(BYTE*)(baseAddr + SHENMUE1_V107_TIME_OF_DAY_DAY);
			month = *(BYTE*)(baseAddr + SHENMUE1_V107_TIME_OF_DAY_MONTH);
			year = *(BYTE*)(baseAddr + SHENMUE1_V107_TIME_OF_DAY_YEAR);
			hour = *(BYTE*)(baseAddr + SHENMUE1_V107_TIME_OF_DAY_HOURS);
			minute = *(BYTE*)(baseAddr + SHENMUE1_V107_TIME_OF_DAY_MINUTES);
			seconds = *(BYTE*)(baseAddr + SHENMUE1_V107_TIME_OF_DAY_SECONDS);

			campos.x = *(float*)(baseAddr + SHENMUE1_V107_CAMPOSX);
			campos.y = *(float*)(baseAddr + SHENMUE1_V107_CAMPOSY);
			campos.z = *(float*)(baseAddr + SHENMUE1_V107_CAMPOSZ);

			mapID	= *(char**)(baseAddr + SHENMUE1_V107_AREA_ID_STRING);
			sceneID = *(BYTE*)(baseAddr + SHENMUE1_V107_SCENE_ID);
			entryID = *(BYTE*)(baseAddr + SHENMUE1_V107_ENTRY_ID);

			fieldofview = *(float*)(baseAddr + SHENMUE1_V107_FOV);
			
			sceneID = *(char*)(baseAddr + 0x133D16C);

			if (!has_values_initialized)
			{
				player_money = temp_player_money;
				sega_coins = temp_sega_coins;
				temp_user_cameradistance[0] = temp_cameradistance;
				user_camerastate = temp_camerastate;
				
				temp_fieldofview = fieldofview;
							   
				has_values_initialized = true;
			}

			if (ingame_status)
			{
				if (playerStruct > 0)
				{
					playerpos.x = *(float*)(playerStruct + SHENMUE1_V107_PLAYERPOSX_OFFSET);
					playerpos.y = *(float*)(playerStruct + SHENMUE1_V107_PLAYERPOSY_OFFSET);
					playerpos.z = *(float*)(playerStruct + SHENMUE1_V107_PLAYERPOSZ_OFFSET);
				}
				if (temp_player_money != player_money)
				{
					memcpy((void*)(baseAddr + SHENMUE1_V107_MONEY), &player_money, sizeof(player_money));
				}
				if (temp_sega_coins != sega_coins)
				{
					memcpy((void*)(baseAddr + SHENMUE1_V107_SEGACOINS), &sega_coins, sizeof(sega_coins));
				}
				if (temp_camerastate != user_camerastate && force_camlock)
				{
					memcpy((void*)(baseAddr + SHENMUE1_V107_CAMSTATE), &user_camerastate, sizeof(user_camerastate));
				}
				if (temp_user_cameradistance[0] != temp_cameradistance)
				{
					memcpy((void*)(baseAddr + SHENMUE1_V107_CAMDISTANCE), &temp_user_cameradistance[0], sizeof(float));
				}
				if (temp_fieldofview != fieldofview)
				{
					memcpy((void*)(baseAddr + SHENMUE1_V107_FOV), &temp_fieldofview, sizeof(float));
				}

				//WriteProcessMemoryWrapper(SHENMUE1_V107_60FPS_FIX, &sm1_60fps_fix, sizeof(sm1_60fps_fix));

				if (baseAddr != NULL && force_timemultiplier) {
					unsigned char const * p = reinterpret_cast<unsigned char const *>(&timeMultiplier);
					for (std::size_t i = 0; i != sizeof(float); ++i)
						memset((void*)(baseAddr + SHENMUE1_V107_TIME_MULTIPLIER_1 + i), p[i], 1);
				}

				if (force_60fps)
				{
					memset((void*)(baseAddr + SHENMUE1_V107_200FPS), 1, 1);
				}
				else
				{
					memset((void*)(baseAddr + SHENMUE1_V107_200FPS), 0, 1);
				}
				if (force_60fps2)
				{
					memset((void*)(baseAddr + SHENMUE1_V107_60FPS), 1, 1);
				}
				else
				{
					memset((void*)(baseAddr + SHENMUE1_V107_60FPS), 0, 1);
				}
			}
			else has_values_initialized = false;
		} 
		else if (gameID == 2) 
		{
			fFrameTime = *(float*)(baseAddr + SHENMUE2_V107_FRAMETIME)*1000.0f;
			fFPS = (1000.0f / fFrameTime);

			playerpos.x = *(float*)(baseAddr + SHENMUE2_V107_PLAYERPOSX);
			playerpos.y = *(float*)(baseAddr + SHENMUE2_V107_PLAYERPOSY);
			playerpos.z = *(float*)(baseAddr + SHENMUE2_V107_PLAYERPOSZ);
			playerpos2.x = *(float*)(baseAddr + SHENMUE2_V107_PLAYERPOSX2);
			playerpos2.y = *(float*)(baseAddr + SHENMUE2_V107_PLAYERPOSY2);
			playerpos2.z = *(float*)(baseAddr + SHENMUE2_V107_PLAYERPOSZ2);
			campos.x = *(float*)(baseAddr + SHENMUE2_V107_CAMPOSX);
			campos.y = *(float*)(baseAddr + SHENMUE2_V107_CAMPOSY);
			campos.z = *(float*)(baseAddr + SHENMUE2_V107_CAMPOSZ);
			temp_player_money = *(int*)(baseAddr + SHENMUE2_V107_MONEY);
			temp_sega_coins = *(int*)(baseAddr + SHENMUE2_V107_SEGACOINS);
			temp_cameradistance = *(float*)(baseAddr + SHENMUE2_V107_CAMDISTANCE);
			temp_cameradistance2 = *(float*)(baseAddr + SHENMUE2_V107_CAMDISTANCE2);
			temp_camerastate = *(int*)(baseAddr + SHENMUE2_V107_CAMSTATE);
			temp_freezetime = *(int*)(baseAddr + SHENMUE2_V107_FREEZETIME);

			g_TaskQueue = *(TaskQueue*)(baseAddr + SHENMUE2_V107_FIRST_TASK);
			day = *(BYTE*)(baseAddr + SHENMUE2_V107_TIME_OF_DAY_DAY);
			month = *(BYTE*)(baseAddr + SHENMUE2_V107_TIME_OF_DAY_MONTH);
			year = *(BYTE*)(baseAddr + SHENMUE2_V107_TIME_OF_DAY_YEAR);
			hour = *(BYTE*)(baseAddr + SHENMUE2_V107_TIME_OF_DAY_HOURS);
			minute = *(BYTE*)(baseAddr + SHENMUE2_V107_TIME_OF_DAY_MINUTES);
			seconds = *(BYTE*)(baseAddr + SHENMUE2_V107_TIME_OF_DAY_SECONDS);

			if (enable_npc_logger && !npc_logger_initialized)
			{
				WriteProcessMemoryWrapper(SHENMUE2_V107_ENABLE_NPC_LOGGER_FIX, &npc_logger_fix, sizeof(npc_logger_fix));
				npc_logger_initialized = true;
			}

			WriteProcessMemoryWrapper(SHENMUE2_V107_60FPS_LOCK, &fps_fix, sizeof(fps_fix));

			memcpy((void*)(baseAddr + SHENMUE2_V107_ENABLE_NPC_LOGGER), &enable_npc_logger, sizeof(enable_npc_logger));

			if (baseAddr != NULL && force_timemultiplier) {
				unsigned char const * p = reinterpret_cast<unsigned char const *>(&timeMultiplier);
				for (std::size_t i = 0; i != sizeof(float); ++i)
					memset((void*)(baseAddr + SHENMUE2_V107_TIMEMULTIPLIER + i), p[i], 1);
			}

			if (!has_values_initialized)
			{
				player_money = temp_player_money;
				sega_coins = temp_sega_coins;

				temp_user_cameradistance[0] = temp_cameradistance;
				temp_user_cameradistance[1] = temp_cameradistance2;

				has_values_initialized = true;
			}
			if (ingame_status)
			{
				if (temp_player_money != player_money)
				{
					memcpy((void*)(baseAddr + SHENMUE2_V107_MONEY), &player_money, sizeof(player_money));
				}
				if (temp_sega_coins != sega_coins)
				{
					memcpy((void*)(baseAddr + SHENMUE2_V107_SEGACOINS), &sega_coins, sizeof(sega_coins));
				}
				if (force_camlock) memset((void*)(baseAddr + SHENMUE2_V107_CAMSTATE), 38, 1);

				if (force_freezetime)
					memset((void*)(baseAddr + SHENMUE2_V107_FREEZETIME), 1, 1);
				else memset((void*)(baseAddr + SHENMUE2_V107_FREEZETIME), 0, 1);

				if (temp_user_cameradistance[0] != temp_cameradistance)
				{
					memcpy((void*)(baseAddr + SHENMUE2_V107_CAMDISTANCE), &temp_user_cameradistance[0], sizeof(float));
				}
				if (temp_user_cameradistance[1] != temp_cameradistance2)
				{
					memcpy((void*)(baseAddr + SHENMUE2_V107_CAMDISTANCE2), &temp_user_cameradistance[1], sizeof(float));
				}

				if (force_60fps)
				{
					memset((void*)(baseAddr + SHENMUE2_V107_60FPS), 1, 1);
				}
				else
				{
					memset((void*)(baseAddr + SHENMUE2_V107_60FPS), 0, 1);
				}

				if (force_60fps2)
				{
					memset((void*)(baseAddr + SHENMUE2_V107_60FPS2), 1, 1);
				}
				else
				{
					memset((void*)(baseAddr + SHENMUE2_V107_60FPS2), 0, 1);
				}

				memset((void*)(baseAddr + SHENMUE2_V107_FPS_STEP), 1, 1);
			}
			else has_values_initialized = false;
		}
	}


    ImGuiIO& io = ImGui::GetIO();
    switch (msg)
    {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
    {
        int button = 0;
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
        if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
        if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
        if (!ImGui::IsAnyMouseDown() && ::GetCapture() == NULL)
            ::SetCapture(hwnd);
        io.MouseDown[button] = true;
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        int button = 0;
        if (msg == WM_LBUTTONUP) button = 0;
        if (msg == WM_RBUTTONUP) button = 1;
        if (msg == WM_MBUTTONUP) button = 2;
        io.MouseDown[button] = false;
        if (!ImGui::IsAnyMouseDown() && ::GetCapture() == hwnd)
            ::ReleaseCapture();
        return 0;
    }
    case WM_MOUSEWHEEL:
        io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
        return 0;
    case WM_MOUSEHWHEEL:
        io.MouseWheelH += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
        return 0;
    case WM_MOUSEMOVE:
        io.MousePos.x = (signed short)(lParam);
        io.MousePos.y = (signed short)(lParam >> 16);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam < 256)
            io.KeysDown[wParam] = 1;
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < 256)
            io.KeysDown[wParam] = 0;
        return 0;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam > 0 && wParam < 0x10000)
            io.AddInputCharacter((unsigned short)wParam);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
            return 1;
        return 0;
    }
    return 0;
}

#pragma endregion

