/*
 * oled_fonts.c
 *
 *  Created on: Jan 3, 2015
 *      Author: Baoshi
 */

#include "fonts.h"

#include "font_bitocra_4x7_ascii.h"
#include "font_bitocra_6x11_iso8859_1.h"
#include "font_bitocra_7x13_iso8859_1.h"
/* #include "font_glcd_5x7.h"
#include "font_roboto_8pt_ascii.h"
#include "font_roboto_10pt_ascii.h"
#include "font_tahoma_8pt_ascii.h"
#include "font_terminus_6x12_iso8859_1.h"
#include "font_terminus_8x14_iso8859_1.h"
#include "font_terminus_10x18_iso8859_1.h"
#include "font_terminus_11x22_iso8859_1.h"
#include "font_terminus_12x24_iso8859_1.h"
#include "font_terminus_14x28_iso8859_1.h"
#include "font_terminus_16x32_iso8859_1.h"
#include "font_terminus_bold_8x14_iso8859_1.h"
#include "font_terminus_bold_10x18_iso8859_1.h"
#include "font_terminus_bold_11x22_iso8859_1.h"
#include "font_terminus_bold_12x24_iso8859_1.h"
#include "font_terminus_bold_14x28_iso8859_1.h"
#include "font_terminus_bold_16x32_iso8859_1.h"
#include "font_terminus_6x12_koi8_r.h"
#include "font_terminus_8x14_koi8_r.h"
#include "font_terminus_14x28_koi8_r.h"
#include "font_terminus_16x32_koi8_r.h"
#include "font_terminus_bold_8x14_koi8_r.h"
#include "font_terminus_bold_14x28_koi8_r.h"
#include "font_terminus_bold_16x32_koi8_r.h"
*/

const font_info_t * fonts [ NUM_FONTS ] = {
//&_fonts_glcd_5x7_info,
#ifdef FONTS_ASCII
#define NUM_FONTS NUM_FONTS+0 //baswi changed 4 to 0, and commented 3 fonts below
        /*
         * ascii fonts
         */
//        &_fonts_bitocra_4x7_ascii_info,
//        &_fonts_roboto_8pt_ascii_info,
//        &_fonts_roboto_10pt_ascii_info,
//        &_font_tahoma_8pt_ascii_info,
#endif
#ifdef FONTS_ISO8859
        /*
         * iso8859_1 fonts
         */
        &_fonts_bitocra_6x11_iso8859_1_info,
        &_fonts_bitocra_7x13_iso8859_1_info,
/*        &_fonts_terminus_6x12_iso8859_1_info,
        &_fonts_terminus_8x14_iso8859_1_info,
        &_fonts_terminus_10x18_iso8859_1_info,
        &_fonts_terminus_11x22_iso8859_1_info,
        &_fonts_terminus_12x24_iso8859_1_info,
        &_fonts_terminus_14x28_iso8859_1_info,
        &_fonts_terminus_16x32_iso8859_1_info,
        &_fonts_terminus_bold_8x14_iso8859_1_info,
        &_fonts_terminus_bold_10x18_iso8859_1_info,
        &_fonts_terminus_bold_11x22_iso8859_1_info,
        &_fonts_terminus_bold_12x24_iso8859_1_info,
        &_fonts_terminus_bold_14x28_iso8859_1_info,
        &_fonts_terminus_bold_16x32_iso8859_1_info, */
#endif
#ifdef FONTS_KOI8
        /*
         * koi8_r fonts
         */
/*        &_fonts_terminus_6x12_koi8_r_info,
        &_fonts_terminus_8x14_koi8_r_info,
        &_fonts_terminus_14x28_koi8_r_info,
        &_fonts_terminus_16x32_koi8_r_info,
        &_fonts_terminus_bold_8x14_koi8_r_info,
        &_fonts_terminus_bold_14x28_koi8_r_info,
        &_fonts_terminus_bold_16x32_koi8_r_info */
#endif
    };
