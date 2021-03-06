/**
 * font-awesome (c) creativemarket.com
 * Author: Josh Farr <josh@creativemarket.com>
 */

#include "CairoRenderer.h"
#include "Image.h"
#include "FontAwesomeException.h"

// HarfBuzz
#include <hb.h>
#include <hb-ft.h>
#include <hb-icu.h>

// Cairo
#include <cairo.h>
#include <cairo-ft.h>

// ICU
#include <unicode/unistr.h>

#include <vector>
#include <iostream>
#include <cmath>


CairoRenderer::CairoRenderer(bool debug, bool gracefulEmpty, bool missing) : 
	Renderer(debug, gracefulEmpty, missing) 
{
}


boost::shared_ptr<Image> CairoRenderer::render(const Font & font, const Color & color, const std::wstring & text, const Color & emptyColor) {
	boost::shared_ptr<Image> image;
	cairo_font_face_t * 	cairoFace;
	cairo_matrix_t 			ctm;
	cairo_matrix_t 			fontMatrix;
	cairo_font_options_t *	fontOptions;
	cairo_scaled_font_t * 	scaledFont;
	FT_Face 				scaledFTFace;

	// wrap freetype font face in a cairo face object
	cairoFace = cairo_ft_font_face_create_for_ft_face(font.face(), 0);
	if (cairo_font_face_status(cairoFace) != CAIRO_STATUS_SUCCESS) {
		throw FontAwesomeException("Could not create cairo face!");
	}

	// setup default transformation matrices
	cairo_matrix_init_identity(&ctm);
	cairo_matrix_init_scale(&fontMatrix, font.pointSize(), font.pointSize());

	fontOptions = cairo_font_options_create();
	cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_ON);
	//cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_NONE);
	//cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);
	cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_SUBPIXEL);
	if (cairo_font_options_status(fontOptions) != CAIRO_STATUS_SUCCESS) {
		throw FontAwesomeException("Bad cairo font options!");
	}

	// scale the face
	scaledFont = cairo_scaled_font_create(cairoFace, &fontMatrix, &ctm, fontOptions);
	if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
		throw FontAwesomeException("Could not create scaled font!");
	}
	scaledFTFace = cairo_ft_scaled_font_lock_face(scaledFont);
	if (scaledFTFace == NULL) {
		throw FontAwesomeException("Font could not be locked!");
	}

	hb_font_t * 				harfFont;
	hb_buffer_t * 				harfBuffer;
	hb_feature_t *				harfFeatures 	= NULL;
	size_t						featureCount 	= 0;
	std::vector<hb_feature_t> 	features;
	const char * 				language 		= "en_US.UTF-8";
	const char * 				direction 		= NULL;
	const char * 				script 			= NULL;

	harfFont = hb_ft_font_create(scaledFTFace, NULL);
	//hb_face_t * harfFace;
	//harfFace = hb_ft_face_create(scaledFTFace, NULL);
	harfBuffer = hb_buffer_create();
	hb_buffer_set_unicode_funcs(harfBuffer, hb_icu_get_unicode_funcs());
	// options are: HB_DIRECTION_LTR, HB_DIRECTION_RTL, HB_DIRECTION_TTB
	hb_buffer_set_direction(harfBuffer, hb_direction_from_string (direction, -1));
	// options: HB_SCRIPT_LATIN, HB_SCRIPT_ARABIC, HB_SCRIPT_HAN - see hb-unicode.h
	hb_buffer_set_script(harfBuffer, hb_script_from_string(script, -1));
	hb_buffer_set_language(harfBuffer, hb_language_from_string(language, -1));
	hb_buffer_set_flags(harfBuffer, HB_BUFFER_FLAG_DEFAULT);
	hb_buffer_guess_segment_properties(harfBuffer);

	const char ** shapers = hb_shape_list_shapers();
	if (debug()) {
		std::wcout << L"Available shapers:";
		for (; *shapers; shapers++) {
			std::wcout << " " << *shapers;
		}
		std::wcout << std::endl;
	}

	if (features_.size() > 0) {
		featureCount = features_.size();
		features.reserve(featureCount);
		if (debug()) {
			std::cout << "Attempting to enable [" << featureCount << "] OpenType features." << std::endl;
		}
		for (size_t index = 0; index < featureCount; ++index) {
			hb_feature_t feature;
			features.push_back(feature);
			if (hb_feature_from_string(features_[index].c_str(), -1, &features[index])) {
				if (debug()) {
					std::cout << "Enabled OpenType feature tag [" << features_[index] << "]." << std::endl;
				}
			}
			else {
				if (debug()) {
					std::cout << "Could not enable OpenType feature tag [" << features_[index] << "]." << std::endl;
				}
			}
		}
		if (features.size() > 0) {
			featureCount = features.size();
			harfFeatures = &features[0];
			if (debug()) {
				std::cout << "Enabled [" << featureCount << "] OpenType feature tags." << std::endl;
			}
		}
	}

	// Layout the text
	//hb_buffer_add_utf8(harfBuffer, reinterpret_cast<const char *>(text.c_str()), text.size(), 0, text.size());
	icu::UnicodeString  ucs;
	std::string 		txt;
	ucs = icu::UnicodeString::fromUTF32(reinterpret_cast<const UChar32*>(text.c_str()), text.size());
	txt = ucs.toUTF8String(txt);
	hb_buffer_add_utf8(harfBuffer, txt.c_str(), txt.size(), 0, txt.size());

	hb_shape_full(harfFont, harfBuffer, harfFeatures, featureCount, NULL); //shapers);

	unsigned int 						glyphCount 	= 0;
	hb_glyph_info_t * 					glyphInfo;
	hb_glyph_position_t * 				glyphPosition;
	std::vector<cairo_glyph_t> 			cairoGlyphs;
	size_t 				 				pixelWidth 	= 2; // 1px border on each side
	size_t 				 				pixelHeight = 0;
	size_t 								x 			= 1; // 1st glyph positioned after left border
	cairo_text_extents_t 				cairoGlyphExtent;
	double								baseline;

	glyphInfo 		= hb_buffer_get_glyph_infos(harfBuffer, &glyphCount);
	glyphPosition 	= hb_buffer_get_glyph_positions(harfBuffer, &glyphCount);
	// preallocate space in the vector for all of the glyphs
	cairoGlyphs.reserve(glyphCount);

	// layout glyphs
	for (size_t index = 0; index < glyphCount; ++index) {
		cairo_glyph_t 		 cairoGlyph;

		cairoGlyph.index 	 = glyphInfo[index].codepoint;
		cairoGlyph.x 		 = x + (glyphPosition[index].x_offset / 64);
		cairoGlyph.y 		 = 0;
		x 					+= glyphPosition[index].x_advance / 64;
		cairoGlyphs[index] 	 = cairoGlyph;
	}

	cairo_scaled_font_glyph_extents(scaledFont, &cairoGlyphs[0], glyphCount, &cairoGlyphExtent);

	pixelWidth 	= cairoGlyphExtent.width + 2;
	pixelHeight = cairoGlyphExtent.height + 2;
	baseline 	= fabs(cairoGlyphExtent.y_bearing) + 1.0;

	if (debug()) {
		std::wcout << L"Glyph baseline at [" << baseline << L"]. X bearing [" << cairoGlyphExtent.x_bearing << L"]" << std::endl;
	}

	if (pixelWidth == 0) {
		pixelWidth = 1;
		if (debug()) {
			std::wcout << L"Zero width image!" << std::endl;
		}
	}
	if (pixelHeight == 0) {
		pixelHeight = 1;
		if (debug()) {
			std::wcout << L"Zero height image!" << std::endl;
		}
	}

	// set vertical positioning & shift x bearing 
	for (size_t index = 0; index < glyphCount; ++index) {
		cairoGlyphs[index].y = baseline;
		cairoGlyphs[index].x -= cairoGlyphExtent.x_bearing;
	}	

	int 				 stride = 0;
	cairo_t * 			 cairoContext;
	cairo_surface_t * 	 cairoSurface;

	image.reset(new Image(pixelWidth, pixelHeight, 32));

	if (debug()) {
		std::wcout << L"Allocated image space [" << pixelWidth << L"x" << pixelHeight << L"]" << std::endl;
	}

	stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, pixelWidth);
	cairoSurface = cairo_image_surface_create_for_data(image->data(), CAIRO_FORMAT_ARGB32, pixelWidth, pixelHeight, stride);
	cairoContext = cairo_create(cairoSurface);
	cairo_set_source_rgba(
		cairoContext, 
		color.floatVal(Color::CHANNEL_BLUE), 
		color.floatVal(Color::CHANNEL_GREEN), 
		color.floatVal(Color::CHANNEL_RED), 
		color.floatVal(Color::CHANNEL_ALPHA)
	);
	cairo_set_scaled_font(cairoContext, scaledFont);
	cairo_set_font_size(cairoContext, font.pointSize());

	if (debug()) {
		std::wcout << L"Rendering [" << glyphCount << L"] glyphs at size [" << font.pointSize() << L"]" << std::endl;
	}

	if (cairo_status(cairoContext) != CAIRO_STATUS_SUCCESS) {
		throw FontAwesomeException("Bad cairo context state!");
	}

	cairo_show_glyphs(cairoContext, &cairoGlyphs[0], glyphCount);

	if (cairo_surface_status(cairoSurface) != CAIRO_STATUS_SUCCESS) {
		throw FontAwesomeException("Bad cairo surface state!");
	}

	// all done! clean up after ourselves.
	cairo_font_options_destroy (fontOptions);
	cairo_font_face_destroy (cairoFace);

	hb_buffer_destroy(harfBuffer);

	cairo_destroy(cairoContext);
	cairo_surface_destroy(cairoSurface);

	return image;
}

void CairoRenderer::features(const std::vector<std::string> & features) {
	features_ = features;
}

