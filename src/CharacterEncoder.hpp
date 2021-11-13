/* Reverse Engineer's Hex Editor
 * Copyright (C) 2021 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef REHEX_CHARACTERENCODER_HPP
#define REHEX_CHARACTERENCODER_HPP

#include <iconv.h>
#include <map>
#include <stdexcept>
#include <stdlib.h>
#include <string>

namespace REHex
{
	/* 4 bytes should be enough for any character... so allow double that. */
	static const size_t MAX_CHAR_SIZE = 8;
	static const char * const DEFAULT_ENCODING = "ASCII";
	
	struct EncodedCharacter
	{
		std::string encoded_char;  /**< Character encoded in chosen encoding (ISO-8859-1, Shift JIS, etc) */
		std::string utf8_char;     /**< Character encoded in UTF-8 */
		
		EncodedCharacter(const std::string &encoded_char, const std::string &utf8_char);
	};
	
	/**
	 * @brief Interface that can encode/decode single characters from an encoding.
	*/
	class CharacterEncoder
	{
		public:
			class InvalidCharacter: public std::runtime_error
			{
				public:
					InvalidCharacter(const std::string &what): runtime_error(what) {}
			};
			
			const size_t word_size;
			
			/**
			 * @brief Decode a single character from a buffer to UTF-8.
			 *
			 * Decodes a single character, up to len bytes in length. If the input
			 * character doesn't occupy the entire buffer, its size can be determined
			 * from encoded_char.size() in the returned EncodedCharacter object.
			 *
			 * Throws an exception of type InvalidCharacter on error.
			*/
			virtual EncodedCharacter decode(const void *data, size_t len) const = 0;
			
			/**
			 * @brief Encode a single character from UTF-8.
			 *
			 * Encodes a single provided UTF-8 character and returns the encoded form
			 * as an EncodedCharacter object.
			 *
			 * Throws an exception of type InvalidCharacter on error.
			*/
			virtual EncodedCharacter encode(const std::string &utf8_char) const = 0;
			
		protected:
			CharacterEncoder(size_t word_size): word_size(word_size) {}
	};
	
	/**
	 * @brief ASCII (7-bit) CharacterEncoder implementation.
	 *
	 * Simply passes through any bytes in the range 0-127 unmodified, throwing an
	 * InvalidCharacter exception if the high bit is set.
	*/
	class CharacterEncoderASCII: public CharacterEncoder
	{
		public:
			CharacterEncoderASCII(): CharacterEncoder(1) {}
			
			virtual EncodedCharacter decode(const void *data, size_t len) const override;
			virtual EncodedCharacter encode(const std::string &utf8_char) const override;
	};
	
	#if 0
	class CharacterEncoder8Bit: public CharacterEncoder
	{
		private:
			const char *to_utf8[256];
			std::map<std::string, unsigned char> from_utf8;
			
		public:
			CharacterEncoder8Bit(const char *utf8_chars[]);
			
			virtual EncodedCharacter decode(const void *data, size_t len) const override;
			virtual EncodedCharacter encode(const std::string &utf8_char) const override;
	};
	#endif
	
	/**
	 * @brief iconv-based CharacterEncoder implementation.
	 *
	 * Handles decoding and encoding of characters using iconv with the given encoding.
	*/
	class CharacterEncoderIconv: public CharacterEncoder
	{
		private:
			std::string encoding;
			
			iconv_t to_utf8;
			iconv_t from_utf8;
			
		public:
			CharacterEncoderIconv(const char *encoding, size_t word_size);
			~CharacterEncoderIconv();
			
			virtual EncodedCharacter decode(const void *data, size_t len) const override;
			virtual EncodedCharacter encode(const std::string &utf8_char) const override;
	};
}

#endif /* !REHEX_CHARACTERENCODER_HPP */