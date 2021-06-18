#include <Windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include "format_types.h"
#include "locking_queue.hpp"

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
void CALLBACK HandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                             LONG idObject, LONG idChild,
                             DWORD dwEventThread, DWORD dwmsEventTime);
HWND createHiddenWindow();
void prepareHID(HWND hwnd);
void mouseSumTimerCallback();

long eventCount = 0;
long totalXPos = 0;
long totalYPos = 0;
bool valorantFocused = false;
HWND hiddenWindow;
USHORT lastButtonFlags = 0;
bool keyWasDown[256] = {false};

LockingQueue<InputPacket> queue;
std::ofstream logFile;

// From https://gist.github.com/e-yes/278302
__int64 getTimeInMS()
{
    FILETIME ft;
    LARGE_INTEGER li;

    /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
     * to a LARGE_INTEGER structure. */
    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;

    __int64 ret = li.QuadPart;
    ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
    ret /= 10000;
    return ret;
}

int main()
{
    hiddenWindow = createHiddenWindow();
    prepareHID(hiddenWindow);
    SetTimer(hiddenWindow, 0, 1000, (TIMERPROC) mouseSumTimerCallback);

    SetWinEventHook(EVENT_SYSTEM_FOREGROUND ,
                    EVENT_SYSTEM_FOREGROUND , NULL,
                    HandleWinEvent, 0, 0,
                    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    std::string name = "log-";
    name.append(std::to_string(getTimeInMS()));
    name.append(".bin");
    logFile.open(name);
    logFile.write(LOG_HEADER, sizeof(LOG_HEADER));
    logFile << (unsigned char) LOG_FORMAT_VERSION;
    logFile.flush();

    std::thread fileOutputThread([]()
    {
        InputPacket packet;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
        while(true)
        {
            queue.waitAndPop(packet);
            int extraSize = 0;
            switch(packet.header.type)
            {
                case KEY_DOWN:
                case KEY_UP: extraSize = sizeof(PacketKeyboard); break;
                case MOUSE_MOVE_TINY: extraSize = sizeof(PacketMouseMoveTiny); break;
                case MOUSE_MOVE_SMALL: extraSize = sizeof(PacketMouseMoveSmall); break;
                case MOUSE_MOVE_MED: extraSize = sizeof(PacketMouseMoveMed); break;
                case MOUSE_BUTTON: extraSize = sizeof(PacketMouseButton); break;
            }
            logFile.write((char*)&packet, sizeof(PacketHeader) + extraSize);
        }
#pragma clang diagnostic pop
    });

    std::cout<<"Ready!"<<std::endl;

    MSG messages;
    while(GetMessage(&messages, nullptr, 0, 0)>0)
    {
        TranslateMessage(&messages);
        DispatchMessage(&messages);
    }

    DeleteObject(hiddenWindow);
    return messages.wParam;
}

HWND createHiddenWindow()
{
    WNDCLASS windowClass = {0};
    windowClass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hInstance = nullptr;
    windowClass.lpfnWndProc = WndProc;
    windowClass.lpszClassName = "Valorant Input Log Hidden";
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    if(!RegisterClass(&windowClass))
        MessageBox(nullptr, "Could not register class", "Error", MB_OK);
    HWND windowHandle = CreateWindow("Valorant Input Log Hidden",
                                     nullptr,
                                     WS_POPUP,
                                     0,
                                     0,
                                     250,
                                     250,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr);
    return windowHandle;
}

void prepareHID(HWND hwnd)
{
    RAWINPUTDEVICE rids[2];

    // Mouse
    rids[0].usUsagePage = 0x01;
    rids[0].usUsage = 0x02;
    rids[0].dwFlags = RIDEV_INPUTSINK;
    rids[0].hwndTarget = hwnd;

    // Keyboard
    rids[1].usUsagePage = 0x01;
    rids[1].usUsage = 0x06;
    rids[1].dwFlags = RIDEV_INPUTSINK;
    rids[1].hwndTarget = hwnd;

    if(RegisterRawInputDevices(rids, 2, sizeof(rids[0])) == false)
    {
        std::cout<<"Registration failed!! "<<GetLastError()<<std::endl;
    }
}

void CALLBACK HandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                             LONG idObject, LONG idChild,
                             DWORD dwEventThread, DWORD dwmsEventTime)
{
    TCHAR wnd_title[256];
    GetWindowTextA(hwnd, wnd_title, 256);

    if(strcmp(wnd_title, "VALORANT  ") == 0)
    {
        if(!valorantFocused)
        {
            valorantFocused = true;
            std::cout << "Logging Valorant inputs..." << std::endl;
        }
    }
    else if(valorantFocused)
    {
        valorantFocused = false;
        std::cout << "No longer logging inputs..." << std::endl;
    }

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_INPUT:
        {
            UINT dwSize = sizeof(RAWINPUT);
            static BYTE lpb[sizeof(RAWINPUT)];

            GetRawInputData((HRAWINPUT)lparam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

            auto* raw = (RAWINPUT*)lpb;
            if(raw->header.dwType == RIM_TYPEMOUSE)
            {
                int xPosRelative = raw->data.mouse.lLastX;
                int yPosRelative = raw->data.mouse.lLastY;

                totalXPos += xPosRelative;
                totalYPos += yPosRelative;

                if(raw->data.mouse.usButtonFlags != lastButtonFlags)
                {
                    lastButtonFlags = raw->data.mouse.usButtonFlags;
                    if(lastButtonFlags != 0 && valorantFocused)
                    {
                        InputPacket packet;
                        packet.header.timestamp = getTimeInMS();
                        packet.header.type = MOUSE_BUTTON;
                        packet.mouseButton.buttonData = lastButtonFlags;
                        queue.push(packet);
                    }
                }
            }
            else if(raw->header.dwType == RIM_TYPEKEYBOARD)
            {
                if((raw->data.keyboard.Message == WM_KEYDOWN || raw->data.keyboard.Message == WM_KEYUP) && raw->data.keyboard.VKey <= 255)
                {
                    bool down = raw->data.keyboard.Message == WM_KEYDOWN;
                    if(keyWasDown[raw->data.keyboard.VKey] != down)
                    {
                        keyWasDown[raw->data.keyboard.VKey] = down;

                        if(valorantFocused)
                        {
                            InputPacket packet;
                            packet.header.timestamp = getTimeInMS();
                            packet.header.type = down ? KEY_DOWN : KEY_UP;
                            packet.keyboard.virtualCode = raw->data.keyboard.VKey;
                            queue.push(packet);
                        }
                    }
                }
            }
            break;
        }
        default:
            return DefWindowProc(hwnd, message, wparam, lparam);
    }
    return 0;
}

void mouseSumTimerCallback()
{
    if((totalXPos != 0 || totalYPos != 0) && valorantFocused)
    {
        InputPacket packet;
        packet.header.timestamp = getTimeInMS();

        if((totalXPos < SCHAR_MAX && totalXPos > SCHAR_MIN) && (totalYPos < SCHAR_MAX && totalYPos > SCHAR_MIN))
        {
            packet.header.type = MOUSE_MOVE_TINY;
            packet.mouseMoveTiny.x = (__int8) totalXPos;
            packet.mouseMoveTiny.y = (__int8) totalYPos;
            packet.mouseMoveTiny.totalInputs = eventCount;
        }
        else if((totalXPos < SHRT_MAX && totalXPos > SHRT_MIN) && (totalYPos < SHRT_MAX && totalYPos > SHRT_MIN))
        {
            packet.header.type = MOUSE_MOVE_SMALL;
            packet.mouseMoveSmall.x = (__int16) totalXPos;
            packet.mouseMoveSmall.y = (__int16) totalYPos;
            packet.mouseMoveSmall.totalInputs = eventCount;
        }
        else if((totalXPos < INT_MAX && totalXPos > INT_MIN) && (totalYPos < INT_MAX && totalYPos > INT_MIN))
        {
            packet.header.type = MOUSE_MOVE_MED;
            packet.mouseMoveMed.x = (__int32) totalXPos;
            packet.mouseMoveMed.y = (__int32) totalYPos;
            packet.mouseMoveMed.totalInputs = eventCount;
        }
        queue.push(packet);
    }
    totalXPos = 0;
    totalYPos = 0;
    SetTimer(hiddenWindow, 0, 50, (TIMERPROC) mouseSumTimerCallback);
}