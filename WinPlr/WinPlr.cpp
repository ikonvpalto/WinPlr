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

HWND hwnd;
HANDLE hData;
FILE_DATA pData;
PCM_DATA pPCM;
FFT_DATA pFFT;
Player::Buffer buf;
Player::Stream stream;
Player::ThreadSystem thread;
Player::DirectGraphic direct;

/*************************************************
* WndProc():
* Window proc method for window class
*************************************************/
LRESULT CALLBACK WndProc(
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
WINAPI 
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

	BOOL isDebug = IsDebuggerPresent();
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
	// load our file to buffer
  	HANDLE_DATA hdData = buf.LoadFileToBuffer(hwnd, hData, pData, pPCM, pFFT);
	// if buffers is empty - exit
	DO_EXIT(
		buf.CheckBufferFile(hdData), 
		"HANDLE_DATA STRUCT ERROR: Can't access to variables"
	);

	// free our handle
	buf.UnloadFileFromBuffer(hdData);

	// set window class 
	LPCSTR lpClass = "WinPlr";
	WNDCLASSEXA wc;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName =  lpClass;
	wc.cbSize = sizeof(WNDCLASSEX);

	// if we can't register window class
	DO_EXIT(
		RegisterClassExA(&wc),
		"Window create error! Can't register main window"
	);

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

	STREAM_DATA streamData;

	//#NOTE: we can choose other types of stream (DirectSound or MME)
#ifdef USE_DX
	if (!strstr(lpCmdLine, "-no_direct_sound"))
		streamData = stream.CreateDirectSoundStream(pData, pPCM, hwnd);
	else
#endif
	streamData = stream.CreateMMIOStream(pData, pPCM, hwnd);
	stream.PlayBufferSound(streamData);

	while (streamData.lpSecondaryDirectBuffer)
	{

	}
	return FALSE;
}


/*************************************************
* WinMain():
* entry-point
*************************************************/
BOOL 
WINAPI 
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
