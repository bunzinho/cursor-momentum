#pragma once

struct Vec2
{
  float x, y;
  Vec2() : x(0.0f), y(0.0f) {}
  Vec2(float x, float y) : x(x), y(y) {}
  Vec2(LONG x, LONG y) : x(static_cast<float>(x)), y(static_cast<float>(y)) {}
  Vec2(POINT p) : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)) {}

  static float length(const Vec2& v)
  {
    return std::sqrt(v.x * v.x + v.y * v.y);
  }

  static float length_squared(const Vec2& v)
  {
    return v.x * v.x + v.y * v.y;
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
