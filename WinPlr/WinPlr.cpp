/*********************************************************
* Copyright (C) VERTVER, 2018. All rights reserved.
* WinPlr - open-source WINAPI audio player.
* MIT-License
**********************************************************
* Module Name: WinAudio entry-point
**********************************************************
* WinPlr.cpp
* Main entry-point
*********************************************************/

#include "WinAudio.h"
#include "WinXAudio.h"
#include "nuklear.h"
#include "nuklear_gdi.h"

HWND hwnd;
HANDLE hXAudioThread;
HANDLE hMutex;
DWORD dwXAudioThreadID;
AUDIO_FILE audioXFile = {};
FILE_DATA pData;
PCM_DATA pPCM;
Player::Buffer fileBuf;
Player::Stream audioStream;
Player::ThreadSystem systemThread;
wchar_t szBuf[48];

/*************************************************
* GetUnicodeString():
* Convert ANSI string to Unicode string
*************************************************/
LPCWSTR 
GetUnicodeStringFromAnsi(
	_In_ LPCSTR lpString
)
{
	size_t uSize = strlen(lpString);

	mbstowcs_s(&uSize, szBuf, lpString, uSize);
	return szBuf;
}

/*************************************************
* WndProc():
* Window proc method for window class
*************************************************/
LRESULT 
CALLBACK 
WndProc(
	_In_ HWND hwnd,
	_In_ UINT message,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(NULL);
		return NULL;
		break;
	default:
		break;
	}

	if (nk_gdi_handle_event(hwnd, message, wParam, lParam))
	{
		return NULL;
	}

	return DefWindowProcA(hwnd, message, wParam, lParam);
}

/*************************************************
* WinMainImpl():
* Implemented entry-point
*************************************************/
BOOL
APIENTRY
WinMainImpl(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	// if hInstance is NULL - exit
	if (hInstance == hPrevInstance)
	{
		ExitProcess(0xFF00FF00);
	}

	HANDLE_DATA hdData = {};
	GdiFont* fontStruct = NULL;
	struct nk_context* nContext = NULL;

	// set zero to structs
	ZeroMemory(&hdData, sizeof(HANDLE_DATA));
	ZeroMemory(&pData, sizeof(FILE_DATA));
	ZeroMemory(&pPCM, sizeof(PCM_DATA));

	BOOL isDebug = IsDebuggerPresent();
	BOOL isNewSystem = IsWindows7OrGreater();

	// set main thread name
	systemThread.ThSetNewThreadName("WINPLR MAIN THREAD");

	if (!strstr(lpCmdLine, "-ignore_startup"))
	{
		// check for SSE instructions 
		// DirectSound can include XMM instructions
		if (!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE))
		{
			ContinueIfYes(
				"Warning! Your CPU doesn't support SSE. Are you sure to continue?",
				"Warning"
			);
		}
	}

	// init COM pointers
	if (isNewSystem)
	{
		R_ASSERT2(CoInitializeEx(NULL, NULL), "Can't init COM Pointers");
	}

	// set window class 
	LPCSTR lpClass = "WinPlr";
	WNDCLASSEXA wc = {};
	ZeroMemory(&wc, sizeof(WNDCLASSEXA));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.lpszClassName =  lpClass;
	wc.cbSize = sizeof(WNDCLASSEXA);

	// if we can't register window class
	DO_EXIT(RegisterClassExA(&wc), "Window create error! Can't register main window");

	// set our window styles
	DWORD dwStyle = WS_OVERLAPPEDWINDOW;

	// set rect
	RECT rc = { 0, 0, 640, 360 };
	HDC hdc = {};
	AdjustWindowRectEx(&rc, dwStyle, FALSE, WS_EX_APPWINDOW);

	// create main window
	hwnd = CreateWindowExA(
		WS_EX_APPWINDOW,
		lpClass,
		"WinPlr 0.2",
		dwStyle | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	// if we can't - exit
	DO_EXIT(hwnd, "Window create error! Can't create main window");

	// get DC from HWND
	hdc = GetDC(hwnd);

	// get font and nk context 
	fontStruct = nk_gdifont_create("Arial", 14);
	nContext = nk_gdi_init(fontStruct, hdc, 640, 360);

	BOOL isRefreshNeedy = TRUE;
	BOOL isRunning = TRUE;

	// show and update our window
	ShowWindow(hwnd, SW_SHOWNORMAL);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	STREAM_DATA streamData = {};
	ZeroMemory(&streamData, sizeof(STREAM_DATA));

	//#TODO: RE-FACTOR this
	while (isRunning)
	{
		// get system message
		MSG sysMsg;
		nk_input_begin(nContext);

		// if we don't need to refresh message - get sys message
		if (!isRefreshNeedy)
		{
			if (GetMessageA(&sysMsg, NULL, 0, 0) <= FALSE)
			{
				isRunning = FALSE;
			}
			else
			{
				TranslateMessage(&sysMsg);
				DispatchMessageA(&sysMsg);
			}
			isRefreshNeedy = TRUE;
		}
		else 
		{ 
			isRefreshNeedy = FALSE;
		}

		while (PeekMessageA(&sysMsg, NULL, 0, 0, PM_REMOVE))
		{
			switch (sysMsg.message)
			{
			case WM_QUIT:
			case WM_DESTROY:
				isRunning = FALSE;
				break;
			}
			TranslateMessage(&sysMsg);
			DispatchMessageW(&sysMsg);
			isRefreshNeedy = TRUE;
		}

		nk_input_end(nContext);

		if (nk_begin(
			nContext,
			"WinPlr Window",
			nk_rect(10, 10, 400, 300),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
			NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			static int iProperty = 20;			
			nk_layout_row_static(nContext, 30, 80, 1);
			if (nk_button_label(nContext, "Load file"))
			{
				if (hdData.dData.lpFile)
				{
					DO_EXIT(HeapFree(fileBuf.hHeap, HEAP_NO_SERIALIZE, hdData.dData.lpFile), "HEAP FAILED");
					hdData.dData.lpFile = NULL;
					hdData.dPCM.lpData = NULL;
				}
					// load our file to buffer
				hdData = fileBuf.LoadFileToBuffer(pData, pPCM);

				// get our vars
				pData = hdData.dData;
				pPCM = hdData.dPCM;
				// if buffers is empty - exit
				DO_EXIT(fileBuf.CheckBufferFile(hdData), "HANDLE_DATA STRUCT ERROR: Can't access to variables");
			}
			nk_layout_row_static(nContext, 30, 80, 1);
			if (nk_button_label(nContext, "Play"))
			{
				if (!hdData.dData.lpFile)
				{
					hdData = fileBuf.LoadFileToBuffer(pData, pPCM);

					pData = hdData.dData;
					pPCM = hdData.dPCM;
					DO_EXIT(fileBuf.CheckBufferFile(hdData), "HANDLE_DATA STRUCT ERROR: Can't access to variables");
				}
				if (strstr(lpCmdLine, "-play_implemented") || !isNewSystem)
				{
					if (!strstr(lpCmdLine, "-no_direct_sound"))
					{
						streamData = audioStream.CreateDirectSoundStream(pData, pPCM, hwnd);
					}
					else
					{
						streamData = audioStream.CreateMMIOStream(pData, pPCM, hwnd);
					}
					audioStream.PlayBufferSound(streamData);
				}
				else
				{
					// setup all needy structs
					XAudioPlayer audioPlay = {};
					XAUDIO_DATA audioStruct = {};
					ZeroMemory(&audioStruct, sizeof(XAUDIO_DATA));
					ZeroMemory(&audioXFile, sizeof(AUDIO_FILE));

					audioXFile.dData = hdData.dData;
					audioXFile.dPCM = hdData.dPCM;

					// begin our thread
					hXAudioThread = CreateThread(
						NULL,
						NULL,
						(LPTHREAD_START_ROUTINE)CreateXAudioThread,
						(LPVOID)&audioXFile,
						NULL,
						&dwXAudioThreadID
					);
				}
			}
			nk_layout_row_static(nContext, 30, 80, 1);
			if (nk_button_label(nContext, "Stop"))
			{
			}
			nk_layout_row_dynamic(nContext, 22, 1);
			nk_property_int(nContext, "Levels", 0, &iProperty, 100, 10, 1);
		}
		nk_end(nContext);
#ifdef DEBUG
		if (nk_begin(
			nContext,
			"Debug info",
			nk_rect(420, 10, 200, 300),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
			NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			std::string szPath = "File path: ";
			if (hdData.dPCM.lpPath)
			{
				szPath = std::string("File path: ") + hdData.dPCM.lpPath;
			}
			std::string szSize = "File size: ";
			if (hdData.dData.dwSize)
			{
				szSize = "File size: " + std::to_string((hdData.dData.dwSize / 1024)) + "KB";
			}
			std::string szChannels = "Sample channels: ";
			if (hdData.dPCM.waveFormat.nChannels)
			{
				szChannels = "Sample channels: " + std::to_string(hdData.dPCM.waveFormat.nChannels);
			}
			std::string szSampleRate = "Sample rate: ";
			if (hdData.dPCM.waveFormat.nSamplesPerSec)
			{ 
				szSampleRate = "Sample rate: " + std::to_string(hdData.dPCM.waveFormat.nSamplesPerSec);
			}
			std::string szBits;
			szBits = "Bits: ";
			if (hdData.dPCM.waveFormat.wBitsPerSample)
			{
				szBits = "Bits: " + std::to_string(hdData.dPCM.waveFormat.wBitsPerSample);
			}
			nk_layout_row_static(nContext, 10, 250, 1);
			nk_label(nContext, (szPath).c_str(), NK_TEXT_LEFT);
			nk_layout_row_static(nContext, 10, 150, 1);
			nk_label(nContext, (szSize).c_str(), NK_TEXT_LEFT);
			nk_layout_row_static(nContext, 10, 150, 1);
			nk_label(nContext, (szChannels).c_str(), NK_TEXT_LEFT);
			nk_layout_row_static(nContext, 10, 150, 1);
			nk_label(nContext, (szSampleRate).c_str(), NK_TEXT_LEFT);
			nk_layout_row_static(nContext, 10, 150, 1);
			nk_label(nContext, (szBits).c_str(), NK_TEXT_LEFT);
		}
		nk_end(nContext);
#endif
		nk_gdi_render(nk_rgb(19, 19, 19)); 
	}

	nk_gdifont_del(fontStruct);
	ReleaseDC(hwnd, hdc);

	// release all buffers
	audioStream.ReleaseSoundBuffers(streamData);

	return FALSE;
}


/*************************************************
* WinMain():
* entry-point
*************************************************/
BOOL 
APIENTRY
WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd 
)
{
	try
	{
		return WinMainImpl(
			hInstance,
			hPrevInstance,
			lpCmdLine,
			nShowCmd
		);
	}
	catch (...)
	{
		CreateErrorText(
			"Stack overflow! Please, restart application"
		);
		return TRUE;
	}
}
