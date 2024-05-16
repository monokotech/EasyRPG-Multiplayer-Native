/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <cstring> // memcpy
#include <cstdint> // uintN_t
#include <iostream>
#include <string>

inline uint8_t ReadU8(const uint8_t *data) {
	return ((uint8_t)data[0] << 0);
}
inline uint16_t ReadU16(const uint8_t *data) {
	return
		((uint16_t)data[0] << 8) | ((uint16_t)data[1] << 0);
}
inline uint32_t ReadU32(const uint8_t *data) {
	return
		((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		((uint32_t)data[2] <<  8) | ((uint32_t)data[3] <<  0);
}

inline int8_t ReadS8(const uint8_t *data) {
	return (int8_t)ReadU8(data);
}
inline int16_t ReadS16(const uint8_t *data) {
	return (int16_t)ReadU16(data);
}
inline int32_t ReadS32(const uint8_t *data) {
	return (int32_t)ReadU32(data);
}

inline void WriteU8(uint8_t *data, uint8_t i) {
	data[0] = (i >> 0) & 0xFF;
}
inline void WriteU16(uint8_t *data, uint16_t i) {
	data[0] = (i >> 8) & 0xFF;
	data[1] = (i >> 0) & 0xFF;
}
inline void WriteU32(uint8_t *data, uint32_t i) {
	data[0] = (i >> 24) & 0xFF;
	data[1] = (i >> 16) & 0xFF;
	data[2] = (i >>  8) & 0xFF;
	data[3] = (i >>  0) & 0xFF;
}

inline void WriteS8(uint8_t *data, int8_t i) {
	WriteU8(data, (uint8_t)i);
}
inline void WriteS16(uint8_t *data, int16_t i) {
	WriteU16(data, (uint16_t)i);
}
inline void WriteS32(uint8_t *data, int32_t i) {
	WriteU32(data, (uint32_t)i);
}

#define MAKE_STREAM_READ_FXN(T, N, S)      \
	inline T Read ## N(std::istream &is) { \
		char buf[S] = {0};                 \
		is.read(buf, sizeof(buf));         \
		return Read ## N((uint8_t*)buf);   \
	}

#define MAKE_STREAM_WRITE_FXN(T, N, S)                \
	inline void Write ## N(std::ostream &os, T val) { \
		char buf[S];                                  \
		Write ## N((uint8_t*)buf, val);               \
		os.write(buf, sizeof(buf));                   \
	}

MAKE_STREAM_READ_FXN(uint8_t, U8, 1);
MAKE_STREAM_READ_FXN(uint16_t, U16, 2);
MAKE_STREAM_READ_FXN(uint32_t, U32, 4);

MAKE_STREAM_READ_FXN(int8_t, S8, 1);
MAKE_STREAM_READ_FXN(int16_t, S16, 2);
MAKE_STREAM_READ_FXN(int32_t, S32, 4);

MAKE_STREAM_WRITE_FXN(uint8_t, U8, 1);
MAKE_STREAM_WRITE_FXN(uint16_t, U16, 2);
MAKE_STREAM_WRITE_FXN(uint32_t, U32, 4);

MAKE_STREAM_WRITE_FXN(int8_t, S8, 1);
MAKE_STREAM_WRITE_FXN(int16_t, S16, 2);
MAKE_STREAM_WRITE_FXN(int32_t, S32, 4);

std::string SerializeString16(std::string_view plain);
std::string DeSerializeString16(std::istream &is);
