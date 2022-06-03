#ifndef BITFIELD_H_
#define BITFIELD_H_

#include <cstddef>
#include <cstdint>

template <class T, size_t Index, size_t Bits = 1>
class BitField {
private:
	enum {
		Mask = (1u << Bits) - 1u
	};

public:
	BitField &operator=(const BitField &rhs) {
		value_ = (value_ & ~Mask) | (rhs.value_ & Mask);
		return *this;
	}

	template <class T2>
	BitField &operator=(T2 value) {
		value_ = (value_ & ~(Mask << Index)) | ((value & Mask) << Index);
		return *this;
	}

	operator T() const {
		return (value_ >> Index) & Mask;
	}

	explicit operator bool() const {
		return (value_ & (Mask << Index)) != 0;
	}

	BitField &operator++() {
		return *this = *this + 1;
	}

	T operator++(int) {
		T r = *this;
		++*this;
		return r;
	}

	BitField &operator--() {
		return *this = *this - 1;
	}

	T operator--(int) {
		T r = *this;
		++*this;
		return r;
	}

private:
	T value_;
};

template <class T, size_t Index>
class BitField<T, Index, 1> {
private:
	enum {
		Bits = 1,
		Mask = 0x01
	};

public:
	BitField &operator=(bool value) {
		value_ = (value_ & ~(Mask << Index)) | (value << Index);
		return *this;
	}

	operator bool() const {
		return (value_ & (Mask << Index)) != 0;
	}

	bool operator!() const {
		return (value_ & (Mask << Index)) == 0;
	}

private:
	T value_;
};

#endif
