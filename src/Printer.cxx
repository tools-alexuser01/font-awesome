/**
 * font-awesome (c) creativemarket.com
 * Author: Josh Farr <josh@creativemarket.com>
 */

#include "Printer.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <unicode/unistr.h>


Printer::Printer(Format format, bool useCodepoints) :
	format_(format),
	codepoints_(useCodepoints)
{
}

std::wstring Printer::mapKeyName(const std::wstring & label) const {
	if (format_ == FORMAT_RAW) {
		std::wstring key = label + std::wstring(L": ");
		return key;
	}
	std::wstring key = label;
	std::replace(key.begin(), key.end(), ' ', '_');
	boost::algorithm::to_lower(key);

	return key;
}


void Printer::printMetrics(const std::wstring & text, const FontInfo & info) const {
	boost::property_tree::wptree pt;

	pt.put(mapKeyName(L"Text Input"), text);
	pt.put(mapKeyName(L"Text Length"), info.textInfo_.length_);
	pt.put(mapKeyName(L"Font Name"), info.name_.c_str());
	pt.put(mapKeyName(L"Font Family"), info.family_.c_str());
	pt.put(mapKeyName(L"Font Style"), info.style_.c_str());
	pt.put(mapKeyName(L"Font Format"), info.format_.c_str());
	pt.put(mapKeyName(L"Available Glyphs"), info.glyphCount_);
	pt.put(mapKeyName(L"Available Sizes"), info.sizes_);

	pt.put(mapKeyName(L"Bold"), info.bold_);
	pt.put(mapKeyName(L"Italic"), info.italic_);
	pt.put(mapKeyName(L"Spline"), info.spline_);
	pt.put(mapKeyName(L"Horizontal"), info.horizontal_);
	pt.put(mapKeyName(L"Vertical"), info.vertical_);
	pt.put(mapKeyName(L"Kerning Available"), info.kerning_);
	pt.put(mapKeyName(L"Scalable"), info.scalable_);
	pt.put(mapKeyName(L"Have Glyph Names"), info.haveGlyphNames_);
	pt.put(mapKeyName(L"Multiple Masters"), info.multipleMasters_);
	pt.put(mapKeyName(L"Max Advance Width"), info.maxAdvance_);

	pt.put(mapKeyName(L"Missing Empty Glyph"), info.textInfo_.missingEmpty_);
	pt.put(mapKeyName(L"Renderable Glyphs"), info.textInfo_.hitCount_);
	pt.put(mapKeyName(L"Non-renderable Glyphs"), info.textInfo_.emptyCount_);
	pt.put(mapKeyName(L"Encoding"), info.encoding_.c_str());


	boost::property_tree::wptree arrayChild;
	boost::property_tree::wptree arrayElement;
	std::wstring childKey;

	std::vector<std::pair<std::wstring, std::wstring> >::const_iterator splineIterator = info.splineNames_.begin();
	if (info.splineNames_.size() > 0) {
		std::wstring splineKeyName = mapKeyName(L"SFNT Names");
		for (; splineIterator != info.splineNames_.end(); ++splineIterator) {
			childKey = mapKeyName(splineIterator->first);
			arrayElement.put_value(splineIterator->second);
			arrayChild.push_back(std::make_pair(childKey, arrayElement));
		}
		pt.put_child(splineKeyName, arrayChild);
		arrayChild.clear();
	}

	std::vector<std::pair<std::wstring, std::wstring> >::const_iterator featureIterator = info.features_.begin();
	if (info.features_.size() > 0) {
		std::wstring featureKeyName = mapKeyName(L"OpenType Features");
		for (; featureIterator != info.features_.end(); ++featureIterator) {
			childKey = mapKeyName(featureIterator->first);
			arrayElement.put_value(featureIterator->second);
			arrayChild.push_back(std::make_pair(childKey, arrayElement));
		}
		pt.put_child(featureKeyName, arrayChild);
		arrayChild.clear();
	}

	std::vector<std::pair<std::wstring, std::wstring> >::const_iterator scriptIterator = info.scripts_.begin();
	if (info.scripts_.size() > 0) {
		std::wstring scriptKeyName = mapKeyName(L"OpenType Scripts");
		for (; scriptIterator != info.scripts_.end(); ++scriptIterator) {
			childKey = mapKeyName(scriptIterator->first);
			arrayElement.put_value(scriptIterator->second);
			arrayChild.push_back(std::make_pair(childKey, arrayElement));
		}
		pt.put_child(scriptKeyName, arrayChild);
		arrayChild.clear();
	}

	std::vector<wchar_t>::const_iterator it = info.charmap_.begin();
	std::wstring charmapKeyName = mapKeyName(L"Character Map");
	if (format_ == FORMAT_XML) {
		childKey = mapKeyName(L"Char");
	}
	else {
		childKey = L"";
	}
	icu::UnicodeString ucs;
	for (; it != info.charmap_.end(); ++it) {
		if (codepoints_) {
			ucs = *it;
			arrayElement.put_value(ucs.char32At(0));
		}
		else {
			arrayElement.put_value(*it);
		}
		arrayChild.push_back(std::make_pair(childKey, arrayElement));
	}
	pt.put_child(charmapKeyName, arrayChild);

	std::wstring glyphKeyName = mapKeyName(L"Glyph Names");
	if (format_ == FORMAT_XML) {
		childKey = mapKeyName(L"Glyph");
	}
	else {
		childKey = L"";
	}
	if (info.haveGlyphNames_) {
		arrayChild.clear();
		std::vector<std::string>::const_iterator glyphIterator = info.glyphNames_.begin();
		for (; glyphIterator != info.glyphNames_.end(); ++glyphIterator) {
			arrayElement.put_value((*glyphIterator).c_str());
			arrayChild.push_back(std::make_pair(childKey, arrayElement));
		}
	}
	pt.put_child(glyphKeyName, arrayChild);

	if (format_ == FORMAT_JSON) {
		boost::property_tree::write_json(std::wcout, pt);
	}
	else if (format_ == FORMAT_XML) {
		boost::property_tree::write_xml(std::wcout, pt);
	}
}

void Printer::printGlyphInfo(const std::string & character, const Glyph & glyph) const {
	boost::property_tree::wptree pt;

	pt.put(mapKeyName(L"Empty"), glyph.empty_);

	if (!glyph.empty_) {
		pt.put(mapKeyName(L"Glyph Character"), character.c_str());
	}

	pt.put(mapKeyName(L"Index"), glyph.index_);

	pt.put(mapKeyName(L"Advance.x"), glyph.advance_.first);
	pt.put(mapKeyName(L"Advance.y"), glyph.advance_.second);

	pt.put(mapKeyName(L"Size.width"), glyph.size_.first);
	pt.put(mapKeyName(L"Size.height"), glyph.size_.second);

	pt.put(mapKeyName(L"Position.x"), glyph.position_.first);
	pt.put(mapKeyName(L"Position.y"), glyph.position_.second);

	if (format_ == FORMAT_JSON) {
		boost::property_tree::write_json(std::wcout, pt);
	}
	else if (format_ == FORMAT_XML) {
		boost::property_tree::write_xml(std::wcout, pt);
	}
}
