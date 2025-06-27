#define NOMINMAX
#include <Windows.h>
#include <vector>
#include <cmath>
#include <chrono>
#include "vec2.h"

using hires_clock = std::chrono::high_resolution_clock;
using namespace std::chrono_literals;
using std::chrono::time_point_cast;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#define WM_TRAYICON WM_USER+1
#define ID_TRAY_QUIT 40001

#ifdef _DEBUG
static wchar_t debug_buffer[128];
#endif


static void handle_tray_icon(HWND hwnd, LPARAM lParam)
{
  switch (lParam)
  {
    case WM_LBUTTONUP:
    {
      break;
    }
    case WM_RBUTTONUP:
    {
      POINT p;
      GetCursorPos(&p);
      HMENU hmenu = CreatePopupMenu();
      AppendMenu(hmenu, MFT_STRING, ID_TRAY_QUIT, TEXT("Quit"));
      SetForegroundWindow(hwnd);
      TrackPopupMenu(hmenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, NULL);
      DestroyMenu(hmenu);
      break;
    }
    case WM_MOUSEMOVE:
    {
      break;
    }
  }
}

hires_clock::time_point last_updated_time{};
Vec2 accumulated_movement{};
Vec2 last_direction{};
float last_length{};
int samples{};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_INPUT:
    {
      auto time_now = hires_clock::now();
      Vec2 last_move{};
      RAWINPUT raw_input{};
      static UINT dwSize = sizeof(RAWINPUT);
      bool is_mouse_relative_move = false;
      Vec2 this_move;
      if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw_input, &dwSize, sizeof(RAWINPUTHEADER)) != -1)
      {
        this_move = Vec2(raw_input.data.mouse.lLastX, raw_input.data.mouse.lLastY);
      }

      if (Vec2::length_squared(this_move) > 0.0f)
      {
        last_move = this_move;
        ++samples;
        if (time_now - last_updated_time <= duration_cast<nanoseconds>(24ms))
        {
          accumulated_movement = Vec2::add(accumulated_movement, last_move);
        }
        else
        {
          // 24ms has passed update and reset
          constexpr auto length_threshold = 6.0;
          last_length = Vec2::length(accumulated_movement);
          if (last_length < length_threshold)
          {
            last_length = 0.0;
          }
          last_length = (last_length / samples) * 4;
          last_direction = Vec2::normalized(accumulated_movement);
          last_updated_time = time_now;
  #ifdef _DEBUG
          swprintf_s(debug_buffer, sizeof(debug_buffer) / 2, L"%llu accumulated: (%f, %f) len: %f %d %f\n", time_now.time_since_epoch().count(), accumulated_movement.x, accumulated_movement.y, last_length, samples, last_length / samples * 4);
          OutputDebugStringW(debug_buffer);
  #endif
          samples = 0;
          accumulated_movement = {};
        }
      }
      return 0;
    }
    case WM_COMMAND:
    {

      if (ID_TRAY_QUIT == LOWORD(wParam))
      {
        PostQuitMessage(0);
      }
      return 0;
    }
    case WM_TRAYICON:
    {
      handle_tray_icon(hwnd, lParam);
      return 0;
    }
    case WM_DESTROY:
    {
      PostQuitMessage(0);
      return 0;
    }
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
  WNDCLASS wc = {
  .lpfnWndProc = WndProc,
  .hInstance = hInstance,
  .lpszClassName = TEXT("CursorMomentum")
  };
  RegisterClass(&wc);

  HWND hwnd = CreateWindowEx(WS_EX_TOOLWINDOW, TEXT("CursorMomentum"), NULL, WS_POPUP,
    0, 0, 0, 0, NULL, NULL, hInstance, NULL);

  NOTIFYICONDATA nid = {
  .cbSize = sizeof(nid),
  .hWnd = hwnd,
  .uID = 1,
  .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
  .uCallbackMessage = WM_TRAYICON,
  .hIcon = LoadIcon(NULL, IDI_APPLICATION),
  .szTip = TEXT("Cursor Momentum")
  };
  Shell_NotifyIcon(NIM_ADD, &nid);

  RAWINPUTDEVICE rid = {
    .usUsagePage = 1,
    .usUsage = 2, // HID_USAGE_GENERIC_MOUSE
    .dwFlags = RIDEV_INPUTSINK,
    .hwndTarget = hwnd
  };

  BOOL success = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
  if (!success)
  {
    MessageBox(0, TEXT("failed to register raw input device"), TEXT("error"), MB_OK);
  }

  BOOL running = true;
  while (true)
  {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
      {
        running = false;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    if (!running)
    {
      break;
    }
    else if (last_length >= 1.0f)
    {
      static std::chrono::nanoseconds accumulated_time{};
      static auto constexpr time_step = std::chrono::duration_cast<std::chrono::nanoseconds>(1s) / 120;

      static auto current_time = hires_clock::now();
      auto new_time = hires_clock::now();

      auto elapse_time = new_time - current_time;
      elapse_time = std::min(elapse_time, time_step * 4);

      accumulated_time += elapse_time;
      while (accumulated_time >= time_step)
      {
        if (new_time - last_updated_time >= duration_cast<nanoseconds>(32ms))
        {
          last_length *= 0.94f;
          Vec2 velocity = last_direction.scale(last_length);
          
          POINT current{};
          GetCursorPos(&current);

          POINT next = Vec2::add(Vec2(current), velocity).POINT();
          SetCursorPos(next.x, next.y);
        }
        accumulated_time -= time_step;
      }
      current_time = new_time;
    }
    else
    {
      WaitMessage();
    }
  } // while (running)
  
  Shell_NotifyIcon(NIM_DELETE, &nid);
}
