
#include <windows.h>
#include "defines.h"
#include "MAssert.h"
#include "MStrSafe.h"
#include "common.hpp"
#ifdef ASSERT_PIPE_ALLOWED
#include "ConEmuCheck.h"
#endif

#ifdef _DEBUG
bool gbInMyAssertTrap = false;
bool gbInMyAssertPipe = false;
//bool gbAllowAssertThread = false;
CEAssertMode gAllowAssertThread = am_Default;
HANDLE ghInMyAssertTrap = NULL;
DWORD gnInMyAssertThread = 0;
#ifdef ASSERT_PIPE_ALLOWED
extern HWND ghConEmuWnd;
#endif
DWORD WINAPI MyAssertThread(LPVOID p)
{
	if (gbInMyAssertTrap)
	{
		// ���� ��� � ����� - �� �������� �� ��������, ����� ����� ���������� ����� ����, ��� ��������� �������
		return IDCANCEL;
	}

	MyAssertInfo* pa = (MyAssertInfo*)p;

	if (ghInMyAssertTrap == NULL)
	{
		ghInMyAssertTrap = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	gnInMyAssertThread = GetCurrentThreadId();
	ResetEvent(ghInMyAssertTrap);
	gbInMyAssertTrap = true;
	
	DWORD nRc = 
		#ifdef CONEMU_MINIMAL
			GuiMessageBox(ghConEmuWnd, pa->szDebugInfo, pa->szTitle, MB_SETFOREGROUND|MB_SYSTEMMODAL|MB_RETRYCANCEL);
		#else
			MessageBoxW(NULL, pa->szDebugInfo, pa->szTitle, MB_SETFOREGROUND|MB_SYSTEMMODAL|MB_RETRYCANCEL);
		#endif
			

	gbInMyAssertTrap = false;
	SetEvent(ghInMyAssertTrap);
	gnInMyAssertThread = 0;
	return nRc;
}
void MyAssertTrap()
{
	_CrtDbgBreak();
}
int MyAssertProc(const wchar_t* pszFile, int nLine, const wchar_t* pszTest)
{
	HANDLE hHeap = GetProcessHeap();
	MyAssertInfo* pa = (MyAssertInfo*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(MyAssertInfo));
	wchar_t *szExeName = (wchar_t*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, (MAX_PATH+1)*sizeof(wchar_t));
	if (!GetModuleFileNameW(NULL, szExeName, MAX_PATH+1)) szExeName[0] = 0;
	msprintf(pa->szTitle, countof(pa->szTitle), L"CEAssert PID=%u TID=%u", GetCurrentProcessId(), GetCurrentThreadId());
	msprintf(pa->szDebugInfo, countof(pa->szDebugInfo), L"Assertion in %s\n%s\n\n%s: %i\n\nPress 'Retry' to trap.",
	                szExeName, pszTest ? pszTest : L"", pszFile, nLine);
	DWORD dwCode = 0;

	if (gAllowAssertThread == am_Thread)
	{
		DWORD dwTID;
		HANDLE hThread = CreateThread(NULL, 0, MyAssertThread, pa, 0, &dwTID);

		if (hThread == NULL)
		{
			dwCode = IDRETRY;
			goto wrap;
		}

		WaitForSingleObject(hThread, INFINITE);
		GetExitCodeThread(hThread, &dwCode);
		CloseHandle(hThread);
		goto wrap;
	}
	
#ifdef ASSERT_PIPE_ALLOWED
#ifdef _DEBUG
	if (gAllowAssertThread == am_Pipe)
	{
		HWND hConWnd = GetConsoleWindow();
		HWND hGuiWnd = ghConEmuWnd;
		#ifndef CONEMU_MINIMAL
		if (hGuiWnd == NULL)
		{
			hGuiWnd = FindWindowEx(NULL, NULL, VirtualConsoleClassMain, NULL);
		}
		#endif
		if (hGuiWnd && !gbInMyAssertTrap)
		{
			gbInMyAssertTrap = true;
			gbInMyAssertPipe = true;
			gnInMyAssertThread = GetCurrentThreadId();
			ResetEvent(ghInMyAssertTrap);
			
			dwCode = GuiMessageBox(hGuiWnd, pa->szDebugInfo, pa->szTitle, MB_SETFOREGROUND|MB_SYSTEMMODAL|MB_RETRYCANCEL);
			gbInMyAssertTrap = false;
			gbInMyAssertPipe = false;
			SetEvent(ghInMyAssertTrap);
			gnInMyAssertThread = 0;
			goto wrap;
			
			//CESERVER_REQ In;
			//ExecutePrepareCmd(&In, CECMD_ASSERT, sizeof(CESERVER_REQ_HDR)+sizeof(MyAssertInfo));
			//memmove(&In.AssertInfo, &a, sizeof(a));
			//wchar_t szGuiPipeName[128];
			//msprintf(szGuiPipeName, countof(szGuiPipeName), CEGUIPIPENAME, (DWORD)hGuiWnd);
			//gbInMyAssertTrap = true;
			//gbInMyAssertPipe = true;
			//CESERVER_REQ* pOut = ExecuteCmd(szGuiPipeName, &In, 1000, hConWnd);
			//gbInMyAssertTrap = false;
			//gbInMyAssertPipe = false;
			//if (pOut)
			//{
			//	if (pOut->hdr.cbSize > sizeof(CESERVER_REQ_HDR))
			//	{
			//		dwCode = pOut->dwData[0];
			//	}
			//	ExecuteFreeResult(pOut);
			//	goto wrap;
			//}
		}
	}

	while (gbInMyAssertPipe)
	{
		Sleep(250);
	}
#endif
#endif

	// � ���������� ����������� ������� ��������� CreateThread(MyAssertThread) ����� ��������
	dwCode = MyAssertThread(pa);

wrap:
	if (pa)
		HeapFree(hHeap, 0, pa);
	if (szExeName)
		HeapFree(hHeap, 0, szExeName);
	return (dwCode == IDRETRY) ? -1 : 1;
}

void MyAssertShutdown()
{
	if (ghInMyAssertTrap != NULL)
	{
		if (gbInMyAssertTrap)
		{
			if (!gnInMyAssertThread)
			{
				_ASSERTE(gbInMyAssertTrap && gnInMyAssertThread);
			}
			else
			{
				HANDLE hHandles[2]; 
				hHandles[0] = OpenThread(SYNCHRONIZE, FALSE, gnInMyAssertThread);
				if (hHandles[0] == NULL)
				{
					_ASSERTE(hHandles[0] != NULL);
				}
				else
				{
					hHandles[1] = ghInMyAssertTrap;
					WaitForMultipleObjects(2, hHandles, FALSE, INFINITE);
					CloseHandle(hHandles[0]);
				}
			}
		}

		CloseHandle(ghInMyAssertTrap);
		ghInMyAssertTrap = NULL;
	}
}
#endif