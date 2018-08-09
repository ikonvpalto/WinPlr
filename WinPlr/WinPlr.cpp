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

HWND hwnd;
HANDLE hData;
FILE_DATA pData;
PCM_DATA pPCM;
FFT_DATA pFFT;
Player::Buffer fileBuf;
Player::Stream audioStream;
Player::ThreadSystem systemThread;

/*************************************************
* WndProc():
* Window proc method for window class
*************************************************/
LRESULT 
CALLBACK 
WndProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
)
{
	switch (message)
	{
	case WM_DESTROY:
#ifdef DEBUG
		MessageBoxA(NULL, "WM_DESTROY proc activated", "Destroy", NULL);
#endif
		PostQuitMessage(0);
		break;
	default:
		break;
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
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd
)
{
	// if hInstance is NULL - exit
	if (!hInstance)
	{
		ExitProcess(0xFF00FF00);
	}

	HANDLE_DATA hdData;

	// set zero to structs
	ZeroMemory(&hdData, sizeof(HANDLE_DATA));
	ZeroMemory(&pData, sizeof(FILE_DATA));
	ZeroMemory(&pPCM, sizeof(PCM_DATA));
	ZeroMemory(&pFFT, sizeof(FFT_DATA));

	BOOL isDebug = IsDebuggerPresent();
	BOOL isNewSystem = IsWindows7OrGreater();

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
		if (!isDebug)
		{
			// take our first message box
			ContinueIfYes(
				"Welcome to WinPlr ver.0.1\nDid you want to start play audio?",
				"Welcome"
			);
		}
		else
		{
#ifdef DEBUG_BREAK
			__debugbreak();
#endif
		}
	}
	// init COM pointers
	if (isNewSystem)
	{
		R_ASSERT2(CoInitializeEx(NULL, NULL), "Can't init COM Pointers");
	}

	// load our file to buffer
	hdData = fileBuf.LoadFileToBuffer(hwnd, hData, pData, pPCM, pFFT);

	// if buffers is empty - exit
	DO_EXIT(
		fileBuf.CheckBufferFile(hdData),
		"HANDLE_DATA STRUCT ERROR: Can't access to variables"
	);

	// set window class 
	LPCSTR lpClass = "WinPlr";
	WNDCLASSEXA wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEXA));
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIconA(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName =  lpClass;
	wc.cbSize = sizeof(WNDCLASSEXA);

	// if we can't register window class
	DO_EXIT(RegisterClassExA(&wc), "Window create error! Can't register main window");

	// set our window styles
	DWORD dwStyle = WS_EX_TOPMOST | WS_EX_APPWINDOW;
	DWORD dwWinStyle = WS_BORDER | WS_VISIBLE;

	// set rect
	RECT rc;
	SetRect(&rc, 0, 0, 640, 360);
	AdjustWindowRect(&rc, dwWinStyle, FALSE);

	if (isDebug)
	{
		dwStyle = NULL;
	}

	// create main window
	hwnd = CreateWindowExA(
		dwStyle,
		lpClass,
		"WinPlr 0.1",
		dwWinStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		640,
		360,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	// if we can't - exit
	DO_EXIT(hwnd, "Window create error! Can't create main window");

	// show and update our window
	ShowWindow(hwnd, SW_SHOWNORMAL);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	// get our vars
	pData = hdData.dData;
	pFFT = hdData.dFFT;
	pPCM = hdData.dPCM;

	if (strstr(lpCmdLine, "-play_implemented") || !isNewSystem)
	{
		STREAM_DATA streamData;
		ZeroMemory(&streamData, sizeof(STREAM_DATA));
		//#NOTE: we can choose other types of stream (DirectSound or MME)
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
		XAudioPlayer audioPlay = {};
		XAUDIO_DATA audioStruct = {};
		ZeroMemory(&audioStruct, sizeof(XAUDIO_DATA));

		AUDIO_FILE audioXFile;
		audioXFile.dData = hdData.dData;
		audioXFile.dPCM = hdData.dPCM;

		systemThread.ThBeginXAudioThread(audioXFile);
	}

	while (TRUE)
	{

	}
	return FALSE;
}


/*************************************************
* WinMain():
* entry-point
*************************************************/
BOOL 
APIENTRY
WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd
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
			"Unknown Error! Please, restart application"
		);
		return TRUE;
	}
}
