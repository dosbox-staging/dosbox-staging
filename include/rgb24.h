#ifndef DOSBOX_RGB24_H
#define DOSBOX_RGB24_H

typedef struct rgb24 {
protected:
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

public:
	constexpr rgb24() {}
	constexpr rgb24(const rgb24 &val) { *this = val; }
	constexpr rgb24(const uint8_t r, const uint8_t g, const uint8_t b)
	        : red(r),
	          green(g),
	          blue(b)
	{}

	constexpr rgb24 &operator=(const rgb24 &) = default;

	constexpr operator int() const
	{
		return (blue << 16) | (green << 8) | (red << 0);
	}

	constexpr static rgb24 byteswap(const rgb24 &in)
	{
		return rgb24(in.blue, in.green, in.red);
	}

} rgb24;

constexpr rgb24 host_to_le(const rgb24 &in) noexcept
{
	return rgb24::byteswap(in);
}

#endif
