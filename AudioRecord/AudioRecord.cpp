// AudioRecord.cpp : ����Ӧ�ó������ڵ㡣
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

// ȫ�ֱ���:
HINSTANCE hInst;                                // ��ǰʵ��
TCHAR szTitle[MAX_LOADSTRING];                  // �������ı�
TCHAR szWindowClass[MAX_LOADSTRING];            // ����������
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
    
    // TODO: �ڴ˷��ô��롣
    MSG msg;
    HACCEL hAccelTable;
    
    CoInitialize(NULL);
    
    // ��ʼ��ȫ���ַ���
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_AUDIORECORD, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    
    // ִ��Ӧ�ó����ʼ��:
    if(!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }
    
    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AUDIORECORD));
    
    // ����Ϣѭ��:
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
//  ����: MyRegisterClass()
//
//  Ŀ��: ע�ᴰ���ࡣ
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
//   ����: InitInstance(HINSTANCE, int)
//
//   Ŀ��: ����ʵ�����������������
//
//   ע��:
//
//        �ڴ˺����У�������ȫ�ֱ����б���ʵ�������
//        ��������ʾ�����򴰿ڡ�
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;
    
    hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����
    
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
//  ����: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  Ŀ��: ���������ڵ���Ϣ��
//
//  WM_COMMAND  - ����Ӧ�ó���˵�
//  WM_PAINT    - ����������
//  WM_DESTROY  - �����˳���Ϣ������
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
            // �����˵�ѡ��:
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
            // TODO: �ڴ���������ͼ����...
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

// �����ڡ������Ϣ�������
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
