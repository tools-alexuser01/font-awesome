/**
 * font-awesome (c) creativemarket.com
 * Author: Josh Farr <josh@creativemarket.com>
 */

#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>
#include <utility> // for pair

/**
 * A FreeType2 Font
 */
class Font {
	public:
		typedef std::pair<signed long, signed long> Vector;

		typedef struct {
				unsigned char * bitmap_;
				Vector advance_;
				Vector size_;
				Vector position_;
				bool empty_;
		} Glyph;

		typedef struct {
			Vector size_;
			Vector min_;
			Vector max_;
		} Range;

		Font(const std::string & filename, int size);
		~Font();

		/**
		 * Get the space required to render a string of text
		 */
		Range size(const std::wstring & text) const;

		/**
		 * Get the rendered glyph for a single character
		 */
		Glyph glyph(wchar_t character, Vector pen) const;

		size_t penDPI() const;
		size_t dpi() const;

	protected:
		void release();

	private:
		FT_Library library_;
		FT_Face face_;
		size_t penDPI_;
		size_t dpi_;
};