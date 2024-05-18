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

#include <cstring>
#include <vector>
#include "crypto.h"
#include "sodium.h"
#include "base64.h"

std::string CryptoErrString(CryptoError err) {
	switch(err) {
	case CryptoError::CE_NO_ERROR:
	case CryptoError::CE_INIT:
		return "libsodium couldn't be initialized.";
	case CryptoError::CE_PWHASH:
		return "call crypto_pwhash() failed.";
	case CryptoError::CE_GENERICHASH:
		return "password hash failed.";
	case CryptoError::CE_PAD:
		return "plain text pad failed.";
	case CryptoError::CE_UNPAD:
		return "plain text unpad failed.";
	case CryptoError::CE_COPY_KEY:
		return "key not copied.";
	case CryptoError::CE_CIPHER_DATA_INVALID:
		return "cipher data is invalid.";
	case CryptoError::CE_ENCRYPT:
		return "encrypt failed.";
	case CryptoError::CE_DECRYPT:
		return "decrypt failed.";
	}
	return "Unknown error.";
}

CryptoError CryptoGetPasswordBase64Hash(std::string_view password, std::string& base64_hash) {
	if (sodium_init() < 0)
		return CryptoError::CE_INIT;
	unsigned char salt[crypto_pwhash_SALTBYTES];
	unsigned char key[crypto_box_SEEDBYTES];
	memset(salt, 0, sizeof(salt));
	int err = crypto_pwhash(key, sizeof(key), password.data(), password.size(), salt,
			crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
			crypto_pwhash_ALG_DEFAULT);
	if (err < 0)
		return CryptoError::CE_PWHASH;
	base64_hash = base64_encode_nopad(std::string(reinterpret_cast<const char*>(key), sizeof(key)));
	return CryptoError::CE_NO_ERROR;
}

CryptoError CryptoEncryptText(std::string_view password, std::string_view plain, std::vector<char>& cipher_data) {
	if (sodium_init() < 0)
		return CryptoError::CE_INIT;
	unsigned char hash[crypto_generichash_BYTES];
	int err = crypto_generichash(hash, sizeof(hash),
			reinterpret_cast<const unsigned char*>(password.data()), password.size(), NULL, 0);
	if (err < 0)
		return CryptoError::CE_GENERICHASH; // password hash failed
	const size_t pad_block_size = 16;
	std::vector<unsigned char> padded_buf(plain.size() + pad_block_size);
	size_t padded_len;
	err = sodium_pad(&padded_len, padded_buf.data(), plain.size(), pad_block_size, padded_buf.size());
	if (err < 0)
		return CryptoError::CE_PAD; // plain text pad failed
	unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
	unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
	std::vector<unsigned char> ciphertext(padded_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
	unsigned long long ciphertext_len;
	randombytes_buf(nonce, sizeof(nonce));
	memcpy(padded_buf.data(), plain.data(), plain.size());
	if (crypto_generichash_BYTES == crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
		memcpy(key, hash, crypto_generichash_BYTES);
	} else {
		return CryptoError::CE_COPY_KEY; // key not copied
	}
	err = crypto_aead_xchacha20poly1305_ietf_encrypt(ciphertext.data(), &ciphertext_len,
					padded_buf.data(), padded_len, NULL, 0, NULL, nonce, key);
	if (err < 0)
		return CryptoError::CE_ENCRYPT; // encrypt failed
	cipher_data.reserve(sizeof(nonce) + ciphertext_len);
	cipher_data.insert(cipher_data.end(), nonce, nonce + sizeof(nonce));
	cipher_data.insert(cipher_data.end(), ciphertext.data(), ciphertext.data() + ciphertext_len);
	return CryptoError::CE_NO_ERROR;
}

CryptoError CryptoDecryptText(std::string_view password, const std::vector<char>& cipher_data, std::string& plain) {
	if (sodium_init() < 0)
		return CryptoError::CE_INIT;
	unsigned char hash[crypto_generichash_BYTES];
	int err = crypto_generichash(hash, sizeof(hash),
			reinterpret_cast<const unsigned char*>(password.data()), password.size(), NULL, 0);
	if (err < 0)
		return CryptoError::CE_GENERICHASH; // password hash failed
	const size_t pad_block_size = 16;
	if (cipher_data.size() < crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
			+ pad_block_size + crypto_aead_xchacha20poly1305_ietf_ABYTES) {
		return CryptoError::CE_CIPHER_DATA_INVALID; // cipher_data is invalid
	}
	unsigned long long decrypted_len = cipher_data.size()
			- crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
			- crypto_aead_xchacha20poly1305_ietf_ABYTES;
	std::vector<unsigned char> decrypted(decrypted_len);
	unsigned long long ciphertext_len = decrypted_len + crypto_aead_xchacha20poly1305_ietf_ABYTES;
	const unsigned char* ciphertext = reinterpret_cast<const unsigned char*>(cipher_data.data())
			+ crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
	unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
	if (crypto_generichash_BYTES == crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
		memcpy(key, hash, crypto_generichash_BYTES);
	} else {
		return CryptoError::CE_COPY_KEY; // key not copied
	}
	const unsigned char* nonce = reinterpret_cast<const unsigned char*>(cipher_data.data());
	err = crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted.data(), NULL, NULL,
					ciphertext, ciphertext_len, NULL, 0, nonce, key);
	if (err < 0)
		return CryptoError::CE_DECRYPT; // decrypt failed
	size_t unpadded_len;
	err = sodium_unpad(&unpadded_len, decrypted.data(), decrypted_len, pad_block_size);
	if (err < 0)
		return CryptoError::CE_UNPAD; // plain text unpad failed
	plain = std::string(reinterpret_cast<const char*>(decrypted.data()), unpadded_len);
	return CryptoError::CE_NO_ERROR;
}
