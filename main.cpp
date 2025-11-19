#include <windows.h>
#include <tchar.h>
#include <process.h>
#include <tlhelp32.h>

// 全局变量
HINSTANCE hInst;
HWND hWnd;
HANDLE hThread = NULL;
bool isRunning = false;

// 进程列表
const TCHAR* processNames[] = {
	_T("ClassManagerApp.exe"),
	_T("ClassManagerCmd.exe"),
	_T("ScreenBcast.exe"),
	_T("RemoteTch.exe"),
	_T("RJRemoteserver.exe"),
	_T("TrayTool.exe"),
};
const int processCount = sizeof(processNames) / sizeof(processNames[0]);

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void StartKillingProcesses();
unsigned int __stdcall KillingThread(void*);
void StopKillingProcesses();

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	hInst = hInstance;

	// 注册窗口类
	WNDCLASSEX wc = {0};
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = _T("ProcessKillerClass");

	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, _T("窗口注册失败！"), _T("错误"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// 创建窗口
	hWnd = CreateWindowEx(
		0,
		_T("ProcessKillerClass"),
		_T("云课堂终结器"),
		WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // 不可调整大小和最大化
		CW_USEDEFAULT, CW_USEDEFAULT, 300, 150,
		NULL, NULL, hInstance, NULL
		);

	if (hWnd == NULL) {
		MessageBox(NULL, _T("窗口创建失败！"), _T("错误"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// 创建按钮
	CreateWindow(_T("BUTTON"), _T("开始"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		50, 50, 80, 30, hWnd, (HMENU)1, hInstance, NULL);

	CreateWindow(_T("BUTTON"), _T("暂停"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		170, 50, 80, 30, hWnd, (HMENU)2, hInstance, NULL);

	// 设置窗口置顶
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);


// 新增：设置定时器，每100ms触发一次，用于刷新置顶状态
	SetTimer(hWnd, 1, 100, NULL);  // 定时器ID为1，间隔100ms

	// 消息循环
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 确保退出前停止线程
	StopKillingProcesses();
	return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case 1: // 开始按钮
			StartKillingProcesses();
			break;
			case 2: // 暂停按钮
			StopKillingProcesses();
			break;
		}
		break;
	case WM_TIMER:
		if (wParam == 1) {  // 匹配定时器ID
			// 强制设置窗口为最顶层，忽略位置和大小变化
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		break;

	case WM_DESTROY:
		// 新增：销毁定时器，释放资源
		KillTimer(hwnd, 1);  // 销毁ID为1的定时器
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// 开始终止进程
void StartKillingProcesses() {
	if (!isRunning && hThread == NULL) {
		isRunning = true;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &KillingThread, NULL, 0, NULL);
		SetWindowText(hWnd, _T("云课堂终结器 - 运行中"));
	}
}

// 终止进程的线程函数
unsigned int __stdcall KillingThread(void* data) {
	while (isRunning) {
		// 循环终止指定进程
		for (int i = 0; i < processCount; i++) {
			if (!isRunning) break; // 检查是否需要停止

			// 创建进程信息结构体
			PROCESSENTRY32 processInfo;
			processInfo.dwSize = sizeof(PROCESSENTRY32);  // 必须初始化结构体大小

			// 创建进程快照
			HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
			if (processesSnapshot == INVALID_HANDLE_VALUE) continue;

			// 遍历进程
			if (Process32First(processesSnapshot, &processInfo)) {  // 检查函数调用是否成功
				if (!_tcscmp(processInfo.szExeFile, processNames[i])) {
					HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processInfo.th32ProcessID);
					if (hProcess != NULL) {
						TerminateProcess(hProcess, 0);
						CloseHandle(hProcess);
					}
				}

				while (Process32Next(processesSnapshot, &processInfo)) {
					if (!isRunning) break; // 检查是否需要停止

					if (!_tcscmp(processInfo.szExeFile, processNames[i])) {
						HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processInfo.th32ProcessID);
						if (hProcess != NULL) {
							TerminateProcess(hProcess, 0);
							CloseHandle(hProcess);
						}
					}
				}
			}

			CloseHandle(processesSnapshot);
		}

		// 短暂休眠，减少CPU占用
		if (isRunning) Sleep(100);
	}
	return 0;
}

// 停止终止进程
void StopKillingProcesses() {
	if (isRunning && hThread != NULL) {
		isRunning = false;
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		hThread = NULL;
		SetWindowText(hWnd, _T("云课堂终结器 - 已暂停"));
	}
}

