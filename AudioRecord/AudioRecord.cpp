// AudioRecord.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "AudioRecord.h"
#include <objbase.h>
#include "AudioManager.h"

#define MAX_LOADSTRING 100

#define START_CAPTURE_ID    200
#define STOP_CAPTURE_ID     201
#define START_PLAY_ID       202
#define STOP_PLAY_ID        203

// 全局变量:
HINSTANCE hInst;                                // 当前实例
TCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
TCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
HINSTANCE g_hInstance = NULL;
HWND g_hWndMain = NULL;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

VOID StartRecord();
VOID StopRecord();
VOID StartPlay();
VOID StopPlay();

class CPlaybackEventHandler: public IPlaybackEvent
{
public:
    virtual VOID OnPlaybackEnd()
    {
        StopPlay();
    }
};

CPlaybackEventHandler g_playbackEventHandler;
CAudioManager g_audioMgr(&g_playbackEventHandler);


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR    lpCmdLine,
                       _In_ int       nCmdShow)
{
    g_hInstance = hInstance;
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    
    // TODO: 在此放置代码。
    MSG msg;
    HACCEL hAccelTable;
    
    CoInitialize(NULL);
    
    // 初始化全局字符串
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_AUDIORECORD, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    
    // 执行应用程序初始化:
    if(!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }
    
    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AUDIORECORD));
    
    // 主消息循环:
    while(GetMessage(&msg, NULL, 0, 0))
    {
        if(!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    CoUninitialize();
    
    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目的: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;
    
    wcex.cbSize = sizeof(WNDCLASSEX);
    
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AUDIORECORD));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_AUDIORECORD);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    
    return RegisterClassEx(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目的: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;
    
    hInst = hInstance; // 将实例句柄存储在全局变量中
    
    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
                        CW_USEDEFAULT, 0, 280, 180, NULL, NULL, hInstance, NULL);
                        
    if(!hWnd)
    {
        return FALSE;
    }
    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    
    return TRUE;
}

VOID CreateControlButtons(HWND hWndParent)
{
    const INT nButtonWidth = 100;
    const INT nButtonHeight = 30;
    const DWORD dwButtonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT;
    
    HWND hWndStartRecord = CreateWindow(_T("BUTTON"), _T("Start Capture"), dwButtonStyle,
                                        20, 20, nButtonWidth, nButtonHeight, hWndParent, (HMENU)START_CAPTURE_ID, hInst, NULL);
                                        
    HWND hWndStopRecord = CreateWindow(_T("BUTTON"), _T("Stop Capture"), dwButtonStyle,
                                       150, 20, nButtonWidth, nButtonHeight, hWndParent, (HMENU)STOP_CAPTURE_ID, hInst, NULL);
                                       
    HWND hWndStartPlay = CreateWindow(_T("BUTTON"), _T("Start Play"), dwButtonStyle,
                                      20, 80, nButtonWidth, nButtonHeight, hWndParent, (HMENU)START_PLAY_ID, hInst, NULL);
                                      
    HWND hWndStopPlay = CreateWindow(_T("BUTTON"), _T("Stop Play"), dwButtonStyle,
                                     150, 80, nButtonWidth, nButtonHeight, hWndParent, (HMENU)STOP_PLAY_ID, hInst, NULL);
}

VOID UpdateButtonStatus(BOOL bEnableStartCapture, BOOL bEnableStopCapture, BOOL bEnableStartPlay, BOOL bEnableStopPlay)
{
    HWND hWndStartRecord = GetDlgItem(g_hWndMain, START_CAPTURE_ID);
    HWND hWndStopRecord = GetDlgItem(g_hWndMain, STOP_CAPTURE_ID);
    HWND hWndStartPlay = GetDlgItem(g_hWndMain, START_PLAY_ID);
    HWND hWndStopPlay = GetDlgItem(g_hWndMain, STOP_PLAY_ID);
    
    EnableWindow(hWndStartRecord, bEnableStartCapture);
    EnableWindow(hWndStopRecord, bEnableStopCapture);
    EnableWindow(hWndStartPlay, bEnableStartPlay);
    EnableWindow(hWndStopPlay, bEnableStopPlay);
}

VOID StartRecord()
{
    if(g_audioMgr.StartCapture())
    {
        UpdateButtonStatus(FALSE, TRUE, FALSE, FALSE);
    }
}

VOID StopRecord()
{
    g_audioMgr.StopCapture();
    UpdateButtonStatus(TRUE, FALSE, TRUE, TRUE);
}

VOID StartPlay()
{
    if(g_audioMgr.CanPlay())
    {
        if(g_audioMgr.StartPlayback())
        {
            UpdateButtonStatus(FALSE, FALSE, FALSE, TRUE);
        }
    }
}

VOID StopPlay()
{
    g_audioMgr.StopPlayback();
    UpdateButtonStatus(TRUE, FALSE, TRUE, FALSE);
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;
    
    switch(message)
    {
        case WM_CREATE:
        {
            g_hWndMain = hWnd;
            CreateControlButtons(hWnd);
            UpdateButtonStatus(TRUE, FALSE, FALSE, FALSE);
        }
        break;
        
        case WM_COMMAND:
            wmId    = LOWORD(wParam);
            wmEvent = HIWORD(wParam);
            // 分析菜单选择:
            switch(wmId)
            {
                case START_CAPTURE_ID:
                    StartRecord();
                    break;
                    
                case STOP_CAPTURE_ID:
                    StopRecord();
                    break;
                    
                case START_PLAY_ID:
                    StartPlay();
                    break;
                    
                case STOP_PLAY_ID:
                    StopPlay();
                    break;
                    
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                    
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
            
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此添加任意绘图代码...
            EndPaint(hWnd, &ps);
            break;
            
        case WM_DESTROY:
        {
            if(g_audioMgr.IsCapturing())
            {
                g_audioMgr.StopCapture();
            }
            if(g_audioMgr.IsPlaybacking())
            {
                g_audioMgr.StopPlayback();
            }
        }
        PostQuitMessage(0);
        break;
        
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch(message)
    {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;
            
        case WM_COMMAND:
            if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
