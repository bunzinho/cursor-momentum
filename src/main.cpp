#include <Windows.h>
#include <vector>
#include <cmath>
#include <chrono>

#ifdef _DEBUG
static wchar_t debug_buffer[128];
#endif

using hires_clock = std::chrono::high_resolution_clock;
using namespace std::chrono_literals;

struct Vec2
{
	float x, y;
	Vec2() : x(0.0), y(0.0) {}
	Vec2(float x, float y) : x(x), y(y) {}
	Vec2(LONG x, LONG y) : x(static_cast<float>(x)), y(static_cast<float>(y)) {}
	Vec2(POINT p) : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)) {}

	static float length(const Vec2& v)
	{
		return std::sqrt(v.x * v.x + v.y * v.y);
	}

	static Vec2 normalized(Vec2 v)
	{
		float len = Vec2::length(v);
		if (len > 0.0)
		{
			v.x /= len;
			v.y /= len;
		}
		return v;
	}
	static Vec2 add(const Vec2& v1, const Vec2& v2)
	{
		return { v1.x + v2.x, v1.y + v2.y };
	}

	Vec2 scale(float scale) const
	{
		return { x * scale, y * scale };
	}

	POINT POINT() const
	{
		
		return { static_cast<LONG>(std::round(x)), static_cast<LONG>(std::round(y)) };
	}
};

static void handle_tray_icon(LPARAM lParam)
{
	switch (lParam)
	{
		case WM_LBUTTONDOWN:
		{
			PostQuitMessage(0);
			break;
		}
		case WM_RBUTTONDOWN:
		{
			break;
		}
		case WM_MOUSEMOVE:
		{
			break;
		}
	}
}

static Vec2 raw_input_to_vec2(HRAWINPUT lParam)
{
	Vec2 out{};
	
	static UINT dwSize = sizeof(RAWINPUT);
	RAWINPUT raw_input{};

	if (GetRawInputData(lParam, RID_INPUT, &raw_input, &dwSize, sizeof(RAWINPUTHEADER)) != -1)
	{
		if (raw_input.header.dwType == RIM_TYPEMOUSE)
		{
			out = { raw_input.data.mouse.lLastX, raw_input.data.mouse.lLastY };
		}
	}
	return out;
}

ULONGLONG last_updated_time{};
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
			ULONGLONG time_now = GetTickCount64();
			Vec2 last_move{};
			RAWINPUT raw_input{};
			static UINT dwSize = sizeof(RAWINPUT);
			bool is_mouse_relative_move = false;
			Vec2 this_move;
			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw_input, &dwSize, sizeof(RAWINPUTHEADER)) != -1)
			{
				this_move = Vec2(raw_input.data.mouse.lLastX, raw_input.data.mouse.lLastY);
			}

			if (Vec2::length(this_move) != 0)
			{
				last_move = this_move;
				++samples;
				if (last_updated_time >= time_now-16)
				{
					accumulated_movement = Vec2::add(accumulated_movement, last_move);
				}
				else
				{
					// 16ms has passed update and reset
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
					swprintf_s(debug_buffer, sizeof(debug_buffer) / 2, L"%lld accumulated: (%f, %f) len: %f %d %f\n", time_now, accumulated_movement.x, accumulated_movement.y, last_length, samples, last_length/samples*4);
					OutputDebugStringW(debug_buffer);
	#endif
					samples = 0;
					accumulated_movement = {};
				}
			}
			return 0;
		}
		case WM_USER+1:
		{
			handle_tray_icon(lParam);
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
	.uCallbackMessage = WM_USER+1,
	.hIcon = LoadIcon(NULL, IDI_APPLICATION),
	.szTip = TEXT("Mouse Momentum")
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
	while (running)
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

		if (last_length >= 1.0f)
		{
			static std::chrono::nanoseconds accumulated_time{};
			static auto current_time = hires_clock::now();
			static auto constexpr time_step = std::chrono::duration_cast<std::chrono::nanoseconds>(1s) / 120;
			auto new_time = hires_clock::now();
			auto elapse_time = new_time - current_time;
			accumulated_time += elapse_time;
			while (accumulated_time >= time_step)
			{
				if (GetTickCount64() >= last_updated_time+24)
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
	}
}
