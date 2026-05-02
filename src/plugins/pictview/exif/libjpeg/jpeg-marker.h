/* jpeg-marker.h
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __JPEG_MARKER_H__
#define __JPEG_MARKER_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
        JPEG_MARKER_SOF0        = 0xc0,
        JPEG_MARKER_SOF1        = 0xc1,
        JPEG_MARKER_SOF2        = 0xc2,
        JPEG_MARKER_SOF3        = 0xc3,
	JPEG_MARKER_DHT		= 0xc4,
        JPEG_MARKER_SOF5        = 0xc5,
        JPEG_MARKER_SOF6        = 0xc6,
        JPEG_MARKER_SOF7        = 0xc7,
	JPEG_MARKER_JPG		= 0xc8,
        JPEG_MARKER_SOF9        = 0xc9,
        JPEG_MARKER_SOF10       = 0xca,
        JPEG_MARKER_SOF11       = 0xcb,
	JPEG_MARKER_DAC		= 0xcc,
        JPEG_MARKER_SOF13       = 0xcd,
        JPEG_MARKER_SOF14       = 0xce,
        JPEG_MARKER_SOF15       = 0xcf,
        JPEG_MARKER_RST0	= 0xd0,
        JPEG_MARKER_RST1	= 0xd1,
        JPEG_MARKER_RST2	= 0xd2,
        JPEG_MARKER_RST3	= 0xd3,
        JPEG_MARKER_RST4	= 0xd4,
	JPEG_MARKER_RST5	= 0xd5,
        JPEG_MARKER_RST6	= 0xd6,
        JPEG_MARKER_RST7	= 0xd7,
        JPEG_MARKER_SOI         = 0xd8,
        JPEG_MARKER_EOI         = 0xd9,
        JPEG_MARKER_SOS         = 0xda,
        JPEG_MARKER_DQT		= 0xdb,
        JPEG_MARKER_DNL		= 0xdc,
        JPEG_MARKER_DRI		= 0xdd,
        JPEG_MARKER_DHP		= 0xde,
        JPEG_MARKER_EXP		= 0xdf,
	JPEG_MARKER_APP0	= 0xe0,
        JPEG_MARKER_APP1        = 0xe1,
	JPEG_MARKER_APP2        = 0xe2,
	JPEG_MARKER_APP3        = 0xe3,
	JPEG_MARKER_APP4        = 0xe4,
	JPEG_MARKER_APP5        = 0xe5,
	JPEG_MARKER_APP6        = 0xe6,
	JPEG_MARKER_APP7        = 0xe7,
	JPEG_MARKER_APP8        = 0xe8,
	JPEG_MARKER_APP9        = 0xe9,
	JPEG_MARKER_APP10       = 0xea,
	JPEG_MARKER_APP11       = 0xeb,
	JPEG_MARKER_APP12       = 0xec,
	JPEG_MARKER_APP13       = 0xed,
	JPEG_MARKER_APP14       = 0xee,
	JPEG_MARKER_APP15       = 0xef,
	JPEG_MARKER_JPG0	= 0xf0,
	JPEG_MARKER_JPG1	= 0xf1,
	JPEG_MARKER_JPG2	= 0xf2,
	JPEG_MARKER_JPG3	= 0xf3,
	JPEG_MARKER_JPG4	= 0xf4,
	JPEG_MARKER_JPG5	= 0xf5,
	JPEG_MARKER_JPG6	= 0xf6,
	JPEG_MARKER_JPG7	= 0xf7,
	JPEG_MARKER_JPG8	= 0xf8,
	JPEG_MARKER_JPG9	= 0xf9,
	JPEG_MARKER_JPG10	= 0xfa,
	JPEG_MARKER_JPG11	= 0xfb,
	JPEG_MARKER_JPG12	= 0xfc,
	JPEG_MARKER_JPG13	= 0xfd,
        JPEG_MARKER_COM         = 0xfe
} JPEGMarker;

#define JPEG_IS_MARKER(m) (((m) >= JPEG_MARKER_SOF0) &&		\
			   ((m) <= JPEG_MARKER_COM))

const char *jpeg_marker_get_name        (JPEGMarker marker);
const char *jpeg_marker_get_description (JPEGMarker marker);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __JPEG_MARKER_H__ */
