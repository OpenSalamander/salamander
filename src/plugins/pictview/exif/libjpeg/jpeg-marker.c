/* jpeg-marker.c
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
#include "config.h"
#include "jpeg-marker.h"

#include <stdlib.h>
#include <crtdbg.h>

static struct {
        JPEGMarker marker;
        const char *name;
        const char *description;
} JPEGMarkerTable[] = {
        {JPEG_MARKER_SOF0, "SOF0", "Encoding (baseline)"},
        {JPEG_MARKER_SOF1, "SOF1", "Encoding (extended sequential)"},
        {JPEG_MARKER_SOF2, "SOF2", "Encoding (progressive)"},
        {JPEG_MARKER_SOF3, "SOF3", "Encoding (lossless)"},
        {JPEG_MARKER_SOF5, "SOF5", "Encoding (differential sequential)"},
        {JPEG_MARKER_SOF6, "SOF6", "Encoding (differential progressive)"},
        {JPEG_MARKER_SOF7, "SOF7", "Encoding (differential lossless)"},
        {JPEG_MARKER_SOF9, "SOF9",
		"Encoding (extended sequential, arithmetic)"},
        {JPEG_MARKER_SOF10, "SOF10", "Encoding (progressive, arithmetic)"},
        {JPEG_MARKER_SOF11, "SOF11", "Encoding (lossless, arithmetic)"},
        {JPEG_MARKER_SOF13, "SOF13",
		"Encoding (differential sequential, arithmetic)"},
        {JPEG_MARKER_SOF14, "SOF14",
		"Encoding (differential progressive, arithmetic)"},
        {JPEG_MARKER_SOF15, "SOF15",
		"Encoding (differential lossless, arithmetic)"},
	{JPEG_MARKER_SOI, "SOI", "Start of image"},
	{JPEG_MARKER_EOI, "EOI", "End of image"},
	{JPEG_MARKER_SOS, "SOS", "Start of scan"},
	{JPEG_MARKER_COM, "COM", "Comment"},
	{JPEG_MARKER_DHT, "DHT", "Define Huffman table"},
	{JPEG_MARKER_JPG, "JPG", "Extension"},
	{JPEG_MARKER_DAC, "DAC", "Define arithmetic coding conditioning"},
	{JPEG_MARKER_RST1, "RST1", "Restart 1"},
	{JPEG_MARKER_RST2, "RST2", "Restart 2"},
	{JPEG_MARKER_RST3, "RST3", "Restart 3"},
	{JPEG_MARKER_RST4, "RST4", "Restart 4"},
	{JPEG_MARKER_RST5, "RST5", "Restart 5"},
	{JPEG_MARKER_RST6, "RST6", "Restart 6"},
	{JPEG_MARKER_RST7, "RST7", "Restart 7"},
	{JPEG_MARKER_DQT, "DQT", "Define quantization table"},
	{JPEG_MARKER_DNL, "DNL", "Define number of lines"},
	{JPEG_MARKER_DRI, "DRI", "Define restart interval"},
	{JPEG_MARKER_DHP, "DHP", "Define hierarchical progression"},
	{JPEG_MARKER_EXP, "EXP", "Expand reference component"},
	{JPEG_MARKER_APP0, "APP0", "Application segment 0"},
	{JPEG_MARKER_APP1, "APP1", "Application segment 1"},
	{JPEG_MARKER_APP2, "APP2", "Application segment 2"},
	{JPEG_MARKER_APP3, "APP3", "Application segment 3"},
	{JPEG_MARKER_APP4, "APP4", "Application segment 4"},
	{JPEG_MARKER_APP5, "APP5", "Application segment 5"},
	{JPEG_MARKER_APP6, "APP6", "Application segment 6"},
	{JPEG_MARKER_APP7, "APP7", "Application segment 7"},
	{JPEG_MARKER_APP8, "APP8", "Application segment 8"},
	{JPEG_MARKER_APP9, "APP9", "Application segment 9"},
	{JPEG_MARKER_APP10, "APP10", "Application segment 10"},
	{JPEG_MARKER_APP11, "APP11", "Application segment 11"},
	{JPEG_MARKER_APP12, "APP12", "Application segment 12"},
	{JPEG_MARKER_APP13, "APP13", "Application segment 13"},
	{JPEG_MARKER_APP14, "APP14", "Application segment 14"},
	{JPEG_MARKER_APP15, "APP15", "Application segment 15"},
	{JPEG_MARKER_JPG0, "JPG0", "Extension 0"},
	{JPEG_MARKER_JPG1, "JPG1", "Extension 1"},
	{JPEG_MARKER_JPG2, "JPG2", "Extension 2"},
	{JPEG_MARKER_JPG3, "JPG3", "Extension 3"},
	{JPEG_MARKER_JPG4, "JPG4", "Extension 4"},
	{JPEG_MARKER_JPG5, "JPG5", "Extension 5"},
	{JPEG_MARKER_JPG6, "JPG6", "Extension 6"},
	{JPEG_MARKER_JPG7, "JPG7", "Extension 7"},
	{JPEG_MARKER_JPG8, "JPG8", "Extension 8"},
	{JPEG_MARKER_JPG9, "JPG9", "Extension 9"},
	{JPEG_MARKER_JPG10, "JPG10", "Extension 10"},
	{JPEG_MARKER_JPG11, "JPG11", "Extension 11"},
	{JPEG_MARKER_JPG12, "JPG12", "Extension 12"},
	{JPEG_MARKER_JPG13, "JPG13", "Extension 13"},
	{0, NULL, NULL}
};

const char *
jpeg_marker_get_name (JPEGMarker marker)
{
	unsigned int i;

	for (i = 0; JPEGMarkerTable[i].name; i++)
		if (JPEGMarkerTable[i].marker == marker)
			break;

	return (JPEGMarkerTable[i].name);
}

const char *
jpeg_marker_get_description (JPEGMarker marker)
{
	unsigned int i;

	for (i = 0; JPEGMarkerTable[i].description; i++)
		if (JPEGMarkerTable[i].marker == marker)
			break;

	return (JPEGMarkerTable[i].description);
}

