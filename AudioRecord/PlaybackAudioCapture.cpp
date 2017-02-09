#include "stdafx.h"
#include "PlaybackAudioCapture.h"
#include "ClassRegister.h"
#include "TimeCostDebug.h"

#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <process.h>
#include <avrt.h>

#define AUDIO_CAPTURE_CLASS _T("audio_cpature_message_class")

enum CAPTURE_STATUS
{
    CAPTURE_START,
    CAPTURE_STOP,
    CAPTURE_ERROR
};

#define WM_CAPTUE_STATUS        WM_USER+100
#define WM_WAVE_FORMAT          WM_USER+101
#define WM_CAPTURE_DATA         WM_USER+102

////////////////////  CPlaybackCaptureImpl  /////////////////
struct capture_thread_data
{
    HANDLE hEventStarted;
    HANDLE hEventStop;
    HWND hWndMessage;
    IMMDevice *pDevice;
};

class CPlaybackCaptureImpl
{
public:
    CPlaybackCaptureImpl();
    ~CPlaybackCaptureImpl();
    
    BOOL Initialize(IPlaybackCaptureEvent *pHandler);
    VOID Destroy();
    
    BOOL Start();
    VOID Stop();
    
    BOOL IsInited() const;
    BOOL IsCapturing() const;
    
    IPlaybackCaptureEvent *GetEventHandler() const
    {
        return m_pEventHandler;
    }
    VOID OnThreadEnd();
    
private:
    IMMDevice *GetDefaultDevice();
    
private:
    HWND m_hWndMessage;
    HANDLE m_hEventStarted;
    HANDLE m_hEventStop;
    IMMDevice *m_pDevice;
    
    HANDLE m_hThreadCapture;
    
    static CClassRegister m_sClassRegister;
    BOOL m_bInited;
    
    IPlaybackCaptureEvent *m_pEventHandler;
};

LRESULT CALLBACK AudioCaptureMessageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    BOOL bHandled(FALSE);
    LRESULT lRet(0);
    
    IPlaybackCaptureEvent *pEventHandler = NULL;
    CPlaybackCaptureImpl *pThis = (CPlaybackCaptureImpl *)GetWindowLong(hWnd, GWL_USERDATA);
    if(pThis != NULL)
    {
        pEventHandler = pThis->GetEventHandler();
    }
    
    switch(uMsg)
    {
        case WM_NCCREATE:
        {
            CREATESTRUCT *pSC = (CREATESTRUCT *)lParam;
            if(pSC != NULL)
            {
                SetWindowLong(hWnd, GWL_USERDATA, (LONG)pSC->lpCreateParams);
            }
        }
        break;
        
        case WM_CAPTUE_STATUS:
        {
            if(wParam == CAPTURE_START)
            {
                if(pEventHandler != NULL)
                {
                    pEventHandler->OnCatpureStart(lParam);
                }
            }
            else if(wParam == CAPTURE_STOP)
            {
                if(pEventHandler != NULL)
                {
                    pEventHandler->OnCaptureStop();
                }
                if(pThis != NULL)
                {
                    pThis->OnThreadEnd();
                }
            }
            
            bHandled = TRUE;
        }
        break;
        
        case WM_WAVE_FORMAT:
        {
            if(pEventHandler != NULL)
            {
                pEventHandler->OnAdjustCaptureFormat((WAVEFORMATEX *)lParam);
            }
            
            bHandled = TRUE;
        }
        break;
        
        case WM_CAPTURE_DATA:
        {
            if(pEventHandler != NULL)
            {
                pEventHandler->OnCatpureData((LPBYTE)lParam, wParam);
            }
            
            bHandled = TRUE;
        }
        break;
        
        default:
            break;
    }
    
    if(!bHandled)
    {
        lRet = ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    
    return lRet;
}

CClassRegister CPlaybackCaptureImpl::m_sClassRegister(AUDIO_CAPTURE_CLASS, AudioCaptureMessageProc);

static VOID NotifyStatus(HWND hWndMesasge, CAPTURE_STATUS eStatus, DWORD dwUserData = 0)
{
    ::SendMessage(hWndMesasge, WM_CAPTUE_STATUS, (WPARAM)eStatus, dwUserData);
}

static VOID NotifyWaveFormat(HWND hWndMessage, WAVEFORMATEX *pFormat)
{
    ::SendMessage(hWndMessage, WM_WAVE_FORMAT, 0, (LPARAM)(WAVEFORMATEX *)pFormat);
}

static VOID NotifyData(HWND hWndMessage, LPBYTE pData, INT nDataLen)
{
    ::SendMessage(hWndMessage, WM_CAPTURE_DATA, nDataLen, (LPARAM)pData);
}

BOOL AdjustFormatTo16Bits(WAVEFORMATEX *pwfx)
{
    BOOL bRet(FALSE);
    
    if(pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        pwfx->wFormatTag = WAVE_FORMAT_PCM;
        pwfx->wBitsPerSample = 16;
        pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
        pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
        
        bRet = TRUE;
    }
    else if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
        if(IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
        {
            pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            pEx->Samples.wValidBitsPerSample = 16;
            pwfx->wBitsPerSample = 16;
            pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
            pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
            
            bRet = TRUE;
        }
    }
    
    return bRet;
}

UINT CaptureAudio(HWND hWndMessage, IMMDevice *pDevice, HANDLE hEventStarted, HANDLE hEventStop)
{
    HRESULT hr;
    IAudioClient *pAudioClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    REFERENCE_TIME hnsDefaultDevicePeriod(0);
    HANDLE hTimerWakeUp = NULL;
    IAudioCaptureClient *pAudioCaptureClient = NULL;
    DWORD nTaskIndex = 0;
    HANDLE hTask = NULL;
    BOOL bStarted(FALSE);
    do
    {
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&pAudioClient);
        if(FAILED(hr))
        {
            break;
        }
        
        hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
        if(FAILED(hr))
        {
            break;
        }
        
        hr = pAudioClient->GetMixFormat(&pwfx);
        if(FAILED(hr))
        {
            break;
        }
        
        if(!AdjustFormatTo16Bits(pwfx))
        {
            break;
        }
        
        hTimerWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
        if(hTimerWakeUp == NULL)
        {
            break;
        }
        
        SetEvent(hEventStarted);
        
        NotifyWaveFormat(hWndMessage, pwfx);
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, 0);
        if(FAILED(hr))
        {
            break;
        }
        
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void **)&pAudioCaptureClient);
        if(FAILED(hr))
        {
            break;
        }
        
        hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
        if(NULL == hTask)
        {
            break;
        }
        
        LARGE_INTEGER liFirstFire;
        liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
        LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
        
        BOOL bOK = SetWaitableTimer(hTimerWakeUp, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE);
        if(!bOK)
        {
            break;
        }
        
        hr = pAudioClient->Start();
        if(FAILED(hr))
        {
            break;
        }
        
        NotifyStatus(hWndMessage, CAPTURE_START, lTimeBetweenFires);
        bStarted = TRUE;
        
        HANDLE waitArray[2] = { hEventStop, hTimerWakeUp };
        DWORD dwWaitResult;
        UINT32 nNextPacketSize(0);
        BYTE *pData = NULL;
        UINT32 nNumFramesToRead;
        DWORD dwFlags;
        while(TRUE)
        {
            dwWaitResult = WaitForMultipleObjects(sizeof(waitArray) / sizeof(waitArray[0]), waitArray, FALSE, INFINITE);
            if(WAIT_OBJECT_0 == dwWaitResult)
            {
                break;
            }
            
            if(WAIT_OBJECT_0 + 1 != dwWaitResult)
            {
                NotifyStatus(hWndMessage, CAPTURE_ERROR);
                break;
            }
            
            hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize);
            if(FAILED(hr))
            {
                NotifyStatus(hWndMessage, CAPTURE_ERROR);
                break;
            }
            
            if(nNextPacketSize == 0)
            {
                continue;
            }
            
            hr = pAudioCaptureClient->GetBuffer(
                     &pData,
                     &nNumFramesToRead,
                     &dwFlags,
                     NULL,
                     NULL
                 );
            if(FAILED(hr))
            {
                NotifyStatus(hWndMessage, CAPTURE_ERROR);
                break;
            }
            
            if(0 != nNumFramesToRead)
            {
                NotifyData(hWndMessage, pData, nNumFramesToRead * pwfx->nBlockAlign);
            }
            
            pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        }
        
    }
    while(FALSE);
    
    if(hTask != NULL)
    {
        AvRevertMmThreadCharacteristics(hTask);
        hTask = NULL;
    }
    
    if(pAudioCaptureClient != NULL)
    {
        pAudioCaptureClient->Release();
        pAudioCaptureClient = NULL;
    }
    
    if(pwfx != NULL)
    {
        CoTaskMemFree(pwfx);
        pwfx = NULL;
    }
    
    if(hTimerWakeUp != NULL)
    {
        CancelWaitableTimer(hTimerWakeUp);
        CloseHandle(hTimerWakeUp);
        hTimerWakeUp = NULL;
    }
    
    if(pAudioClient != NULL)
    {
        if(bStarted)
        {
            pAudioClient->Stop();
            NotifyStatus(hWndMessage, CAPTURE_STOP);
        }
        
        pAudioClient->Release();
        pAudioClient = NULL;
    }
    
    return 0;
}

UINT __stdcall CaptureTheadProc(LPVOID param)
{
    CoInitialize(NULL);
    
    capture_thread_data *pData = (capture_thread_data *)param;
    HWND hWndMessage = pData->hWndMessage;
    HANDLE hEventStop = pData->hEventStop;
    IMMDevice *pDevice = pData->pDevice;
    HANDLE hEventStarted = pData->hEventStarted;
    
    UINT nRet = CaptureAudio(hWndMessage, pDevice, hEventStarted, hEventStop);
    
    CoUninitialize();
    
    return nRet;
}

CPlaybackCaptureImpl::CPlaybackCaptureImpl()
    : m_hWndMessage(NULL), m_bInited(FALSE), m_pDevice(NULL),
      m_pEventHandler(NULL), m_hEventStarted(NULL), m_hEventStop(NULL)
{

}

CPlaybackCaptureImpl::~CPlaybackCaptureImpl()
{
    if(m_bInited)
    {
        Destroy();
    }
}

VOID CPlaybackCaptureImpl::OnThreadEnd()
{
    if(m_hThreadCapture != NULL)
    {
        CloseHandle(m_hThreadCapture);
        m_hThreadCapture = NULL;
    }
}

IMMDevice *CPlaybackCaptureImpl::GetDefaultDevice()
{
    IMMDevice *pDevice = NULL;
    IMMDeviceEnumerator *pMMDeviceEnumerator = NULL;
    HRESULT hr = CoCreateInstance(
                     __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator),
                     (void **)&pMMDeviceEnumerator);
    if(FAILED(hr))
    {
        return NULL;
    }
    
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    pMMDeviceEnumerator->Release();
    
    return pDevice;
}

BOOL CPlaybackCaptureImpl::Initialize(IPlaybackCaptureEvent *pHandler)
{
    if(m_bInited)
    {
        return TRUE;
    }
    
    m_pEventHandler = pHandler;
    
    do
    {
        if(!m_sClassRegister.IsRegistered())
        {
            m_sClassRegister.Register();
        }
        
        m_hWndMessage = CreateWindow(AUDIO_CAPTURE_CLASS, NULL, WS_POPUP,
                                     0, 0, 0, 0, HWND_MESSAGE, NULL, g_hInstance, this);
        if(m_hWndMessage == NULL)
        {
            break;
        }
        
        m_pDevice = GetDefaultDevice();
        if(m_pDevice == NULL)
        {
            break;
        }
        
        m_hEventStop = CreateEvent(NULL, TRUE, FALSE, NULL);
        if(m_hEventStop == NULL)
        {
            break;
        }
        
        m_hEventStarted = CreateEvent(NULL, TRUE, FALSE, NULL);
        if(m_hEventStarted == NULL)
        {
            break;
        }
        
        m_bInited = TRUE;
        
    }
    while(FALSE);
    
    if(!m_bInited)
    {
        Destroy();
    }
    
    return m_bInited;
}

VOID CPlaybackCaptureImpl::Destroy()
{
    if(m_hWndMessage != NULL
            && ::IsWindow(m_hWndMessage))
    {
        DestroyWindow(m_hWndMessage);
    }
    
    m_hWndMessage = NULL;
    
    if(m_pDevice != NULL)
    {
        m_pDevice->Release();
        m_pDevice = NULL;
    }
    
    if(m_hEventStop != NULL)
    {
        CloseHandle(m_hEventStop);
        m_hEventStop = NULL;
    }
    
    if(m_hEventStarted != NULL)
    {
        CloseHandle(m_hEventStarted);
        m_hEventStarted = NULL;
    }
    
    m_bInited = FALSE;
}

BOOL CPlaybackCaptureImpl::IsInited() const
{
    return m_bInited;
}

BOOL CPlaybackCaptureImpl::IsCapturing() const
{
    return m_hThreadCapture != NULL;
}

BOOL CPlaybackCaptureImpl::Start()
{
    if(!m_bInited)
    {
        return FALSE;
    }
    if(m_hThreadCapture != NULL)
    {
        return TRUE;
    }
    
    capture_thread_data data;
    data.hEventStop = m_hEventStop;
    data.hWndMessage = m_hWndMessage;
    data.pDevice = m_pDevice;
    data.hEventStarted = m_hEventStarted;
    
    m_hThreadCapture = (HANDLE)_beginthreadex(NULL, 0, &CaptureTheadProc, &data, 0, NULL);
    if(m_hThreadCapture == NULL)
    {
        return FALSE;
    }
    
    HANDLE arWaits[2] = {m_hEventStarted, m_hThreadCapture};
    DWORD dwWaitResult = WaitForMultipleObjects(sizeof(arWaits) / sizeof(arWaits[0]), arWaits, FALSE, INFINITE);
    if(dwWaitResult != WAIT_OBJECT_0)
    {
        Stop();
        return FALSE;
    }
    
    return TRUE;
}

VOID CPlaybackCaptureImpl::Stop()
{
    if(!m_bInited)
    {
        return;
    }
    
    if(m_hEventStop != NULL
            && m_hThreadCapture != NULL)
    {
        SetEvent(m_hEventStop);
        OnThreadEnd();
    }
}

////////////////////  CPlaybackCaptureImpl  /////////////////


////////////////////  CPlaybackCapture ////////////////

CPlaybackCapture::CPlaybackCapture()
{
    m_pImpl = new CPlaybackCaptureImpl();
}

CPlaybackCapture::~CPlaybackCapture()
{
    if(m_pImpl != NULL)
    {
        delete m_pImpl;
    }
}

BOOL CPlaybackCapture::Initialize(IPlaybackCaptureEvent *pHandler)
{
    if(m_pImpl != NULL)
    {
        return m_pImpl->Initialize(pHandler);
    }
    else
    {
        return FALSE;
    }
}

VOID CPlaybackCapture::Destroy()
{
    if(m_pImpl != NULL)
    {
        m_pImpl->Destroy();
    }
}

BOOL CPlaybackCapture::Start()
{
    if(m_pImpl != NULL)
    {
        return m_pImpl->Start();
    }
    else
    {
        return FALSE;
    }
}

VOID CPlaybackCapture::Stop()
{
    if(m_pImpl != NULL)
    {
        m_pImpl->Stop();
    }
}

BOOL CPlaybackCapture::IsInited() const
{
    if(m_pImpl != NULL)
    {
        return m_pImpl->IsInited();
    }
    else
    {
        return FALSE;
    }
}

BOOL CPlaybackCapture::IsCapturing() const
{
    if(m_pImpl != NULL)
    {
        return m_pImpl->IsCapturing();
    }
    else
    {
        return FALSE;
    }
}

////////////////////  CPlaybackCapture ////////////////