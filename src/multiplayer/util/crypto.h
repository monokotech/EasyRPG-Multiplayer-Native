/*
 * EPMP
 * Copyright (C) 2024 Monokotech
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class CryptoError : uint8_t {
	CE_NO_ERROR,
	CE_INIT,
	CE_PWHASH,
	CE_GENERICHASH,
	CE_PAD,
	CE_COPY_KEY,
	CE_ENCRYPT,
	CE_CIPHER_DATA_INVALID,
	CE_DECRYPT,
	CE_UNPAD,
};

std::string CryptoErrString(CryptoError err);

CryptoError CryptoGetPasswordBase64Hash(std::string_view password, std::string& base64_hash);

CryptoError CryptoEncryptText(std::string_view password, std::string_view plain, std::vector<char>& cipher_data);
CryptoError CryptoDecryptText(std::string_view password, const std::vector<char>& cipher_data, std::string& plain);
