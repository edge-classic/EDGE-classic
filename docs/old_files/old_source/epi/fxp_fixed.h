//------------------------------------------------------------------------
//  EPI Fixed-point type
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2005  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __EPI_FXP_FIXED_H__
#define __EPI_FXP_FIXED_H__

namespace epi
{

class angle_c;

class fix_c
{
	/* sealed class, value semantics */

friend class xvec2_c;
friend class xvec3_c;

private:
	s32_t raw;

	typedef enum
	{
		DUMMY_E = 0
	}
	dummy_e;

	fix_c(s32_t _raw, dummy_e xxx) : raw(_raw) { }
	// xxx is a dummy argument, without it this method's signature
	// becomes too similar to the integer constructors below.

public:
	fix_c() : raw(0) { };
	fix_c(const fix_c& rhs) : raw(rhs.raw) { }

	fix_c(int I);
	fix_c(unsigned int I);
	fix_c(short numer, short denom);  // i.e. fractions

	fix_c(float F);
	fix_c(double F);

	int ToInt() const;   // rounds towards zero
	int Trunc() const;   // rounds towards -inf (faster)
	float ToFloat() const;

	fix_c Frac() const;
	fix_c RawFrac() const;
	
	std::string ToStr(int precision = 5) const;
    static fix_c Parse(const std::string& str);
    static fix_c Parse(const char *);

	std::string ToHexStr() const;
    static fix_c FromHexStr(const std::string& str);
    static fix_c FromHexStr(const char *str);

	/* ---- read-only operations ---- */

	inline bool operator<  (const fix_c& rhs) const { return raw <  rhs.raw; }
	inline bool operator>  (const fix_c& rhs) const { return raw >  rhs.raw; }
	inline bool operator>= (const fix_c& rhs) const { return raw >= rhs.raw; }
	inline bool operator<= (const fix_c& rhs) const { return raw <= rhs.raw; }
	inline bool operator== (const fix_c& rhs) const { return raw == rhs.raw; }
	inline bool operator!= (const fix_c& rhs) const { return raw != rhs.raw; }

	fix_c operator- () const;

	fix_c operator+ (const fix_c& rhs) const;
	fix_c operator- (const fix_c& rhs) const;
	fix_c operator* (const fix_c& rhs) const;
	fix_c operator/ (const fix_c& rhs) const;

	fix_c operator* (int factor) const;
	fix_c operator/ (int factor) const;

	fix_c operator<< (int shift) const;
	fix_c operator>> (int shift) const;

	/* ---- modifying operations ---- */

	fix_c& operator= (const fix_c& rhs);

	fix_c& operator+= (const fix_c& rhs);
	fix_c& operator-= (const fix_c& rhs);
	fix_c& operator*= (const fix_c& rhs);
	fix_c& operator/= (const fix_c& rhs);

	fix_c& operator*= (int factor);
	fix_c& operator/= (int factor);

	fix_c& operator<<= (int shift);
	fix_c& operator>>= (int shift);

	/* ---- useful constants ---- */

	static inline fix_c One()    { return fix_c(65536,  DUMMY_E); }
	static inline fix_c Pi()     { return fix_c(205887, DUMMY_E); }
	static inline fix_c Root2()  { return fix_c(92682,  DUMMY_E); }
	static inline fix_c Tiny()   { return fix_c(1,      DUMMY_E); }

	static inline fix_c MaxVal() { return fix_c( 0x7FFFFFFF, DUMMY_E); }
	static inline fix_c MinVal() { return fix_c(-0x7FFFFFFF, DUMMY_E); }

    /* ---- type-casting operators ---- */

	friend fix_c operator+ (int I, const fix_c& X);
	friend fix_c operator- (int I, const fix_c& X);
	friend fix_c operator* (int I, const fix_c& X);
	friend fix_c operator/ (int I, const fix_c& X);

	friend bool operator<  (int I, const fix_c& X);
	friend bool operator>  (int I, const fix_c& X);
	friend bool operator>= (int I, const fix_c& X);
	friend bool operator<= (int I, const fix_c& X);
	friend bool operator== (int I, const fix_c& X);
	friend bool operator!= (int I, const fix_c& X);

    /* ---- external functions ---- */

    friend fix_c fxabs(const fix_c& X);
    friend fix_c fxfloor(const fix_c& X);
    friend fix_c fxceil(const fix_c& X);

    // computes the result of (A * B / C)
    friend fix_c fxmuldiv(const fix_c& A, const fix_c& B, const fix_c& C);

	friend fix_c fxmax(const fix_c& A, const fix_c& B);
	friend fix_c fxmin(const fix_c& A, const fix_c& B);

    friend fix_c fxsqrt(const fix_c& X);
    friend fix_c fxdist2(const fix_c& X, const fix_c& Y);
    friend fix_c fxdist3(const fix_c& X, const fix_c& Y, const fix_c& Z);

    friend fix_c fxsin(const angle_c& ang);
    friend fix_c fxcos(const angle_c& ang);
    friend fix_c fxtan(const angle_c& ang);

    friend angle_c fxatan(const fix_c& X);
    friend angle_c fxatan2(const fix_c& X, const fix_c& Y);

	// internal routines
    friend fix_c base_sin(u32_t bam);
    friend fix_c base_tan(u32_t bam);
    friend angle_c base_atan(const fix_c& X);

	/* ---- bad shit you should never use ---- */
	static fix_c RawBitsConstruct(s32_t _raw) { return fix_c(_raw, DUMMY_E); }
	s32_t RawBitsRetrieve() const { return raw; }

private:
	static const u32_t sqrt_table[257];
	static const u32_t sine_table[257];
	static const u32_t tangent_table[257];
	static const u32_t arctan_table[257];
};


//------------------------------------------------------------------------
//    IMPLEMENTATION
//------------------------------------------------------------------------

inline fix_c::fix_c(int I) : raw(I << 16)
{ }

inline fix_c::fix_c(unsigned int I) : raw(I << 16)
{ }

inline fix_c::fix_c(short numer, short denom) : raw((int(numer) << 16) / denom)
{ }

inline fix_c::fix_c(float F) : raw((s32_t)(F * 65536.0f))
{ }

inline fix_c::fix_c(double F) : raw((s32_t)(F * 65536.0))
{ }


//------------------------------------------------------------------------

inline int fix_c::ToInt() const
{
    return ((raw < 0) ? (raw + 0xFFFF) : raw) >> 16;
}

inline int fix_c::Trunc() const
{
    return raw >> 16;
}

inline float fix_c::ToFloat() const
{
	return float(raw) / 65536.0f;
}

inline fix_c fix_c::Frac() const
{
	return fix_c(abs(raw) & 0xFFFF, DUMMY_E);
}

inline fix_c fix_c::RawFrac() const
{
    return fix_c(raw & 0xFFFF, DUMMY_E);
}


//------------------------------------------------------------------------

inline fix_c fix_c::operator- () const
{
	return fix_c(-raw, DUMMY_E);
}

inline fix_c fix_c::operator+ (const fix_c& rhs) const
{
	return fix_c(raw + rhs.raw, DUMMY_E);
}

inline fix_c fix_c::operator- (const fix_c& rhs) const
{
	return fix_c(raw - rhs.raw, DUMMY_E);
}

inline fix_c fix_c::operator* (const fix_c& rhs) const
{
    fix_c temp(*this); temp *= rhs; return temp;
}

inline fix_c fix_c::operator/ (const fix_c& rhs) const
{
    fix_c temp(*this); temp /= rhs; return temp;
}

inline fix_c fix_c::operator* (int factor) const
{
    return fix_c(raw * factor, DUMMY_E);
}

inline fix_c fix_c::operator/ (int factor) const
{
    return fix_c(raw / factor, DUMMY_E);
}

inline fix_c fix_c::operator<< (int shift) const
{
    return fix_c(raw << shift, DUMMY_E);
}

inline fix_c fix_c::operator>> (int shift) const
{
    return fix_c(raw >> shift, DUMMY_E);
}

//------------------------------------------------------------------------

inline fix_c& fix_c::operator= (const fix_c& rhs)
{
	// no need to check for self assignment
	raw = rhs.raw;
	return *this;
}

inline fix_c& fix_c::operator+= (const fix_c& rhs)
{
	raw += rhs.raw;
	return *this;
}

inline fix_c& fix_c::operator-= (const fix_c& rhs)
{
	raw -= rhs.raw;
	return *this;
}

inline fix_c& fix_c::operator*= (const fix_c& rhs)
{
	raw = ((i64_t)raw * (i64_t)rhs.raw) >> 16;
	return *this;
}

inline fix_c& fix_c::operator/= (const fix_c& rhs)
{
	raw = ((i64_t)raw << 16) / (i64_t)rhs.raw;
	return *this;
}

inline fix_c& fix_c::operator*= (int factor)
{
	raw *= factor;
	return *this;
}

inline fix_c& fix_c::operator/= (int factor)
{
	raw /= factor;
	return *this;
}

inline fix_c& fix_c::operator<<= (int shift)
{
	raw <<= shift;
	return *this;
}

inline fix_c& fix_c::operator>>= (int shift)
{
	raw >>= shift;
	return *this;
}

//------------------------------------------------------------------------

inline fix_c operator+ (int I, const fix_c& X)
{
	return fix_c(I) + X;
}

inline fix_c operator- (int I, const fix_c& X)
{
	return fix_c(I) - X;
}

inline fix_c operator* (int I, const fix_c& X)
{
	return fix_c(I) * X;
}

inline fix_c operator/ (int I, const fix_c& X)
{
	return fix_c(I) / X;
}

inline bool operator<  (int I, const fix_c& X)
{
	return fix_c(I) < X;
}

inline bool operator>  (int I, const fix_c& X)
{
	return fix_c(I) > X;
}

inline bool operator>= (int I, const fix_c& X)
{
	return fix_c(I) >= X;
}

inline bool operator<= (int I, const fix_c& X)
{
	return fix_c(I) <= X;
}

inline bool operator== (int I, const fix_c& X)
{
	return fix_c(I) == X;
}

inline bool operator!= (int I, const fix_c& X)
{
	return fix_c(I) != X;
}

//------------------------------------------------------------------------

inline fix_c fxabs(const fix_c& X)
{
	return fix_c(X.raw < 0 ? -X.raw : X.raw, fix_c::DUMMY_E);
}

inline fix_c fxfloor(const fix_c& X)
{
    return fix_c(X.raw & ~0xFFFF, fix_c::DUMMY_E);
}

inline fix_c fxceil(const fix_c& X)
{
	// prevent overflow
	if (X.raw >= 0x7FFF0000)
		return fix_c::MaxVal();

    return fix_c((X.raw | 0xFFFF) + 1, fix_c::DUMMY_E);
}

inline fix_c fxmax(const fix_c& A, const fix_c& B)
{
	return (A.raw > B.raw) ? A : B;
}

inline fix_c fxmin(const fix_c& A, const fix_c& B)
{
	return (A.raw < B.raw) ? A : B;
}

inline fix_c fxdist3(const fix_c& X, const fix_c& Y, const fix_c& Z)
{
    return fxdist2(fxdist2(X, Y), Z);
}

}  // namespace epi

#endif /* __EPI_FXP_FIXED_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
