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

#include "serialize.h"

std::string SerializeString16(std::string_view plain) {
	std::string s;
	char buf[2];

	if (plain.size() > 0xFFFF) // string too long
		return std::string("\x00\x00", 2);
	s.reserve(2 + plain.size());

	WriteU16((uint8_t*)&buf[0], plain.size());
	s.append(buf, 2);

	s.append(plain);
	return s;
}

std::string DeSerializeString16(std::istream &is) {
	std::string s;
	char buf[2];

	is.read(buf, 2);
	if (is.gcount() != 2) // size not read
		return "";

	uint16_t s_size = ReadU16((uint8_t*)buf);
	if (s_size == 0)
		return s;

	s.resize(s_size);
	is.read(&s[0], s_size);
	if (is.gcount() != s_size) // couldn't read all chars
		return "";

	return s;
}
