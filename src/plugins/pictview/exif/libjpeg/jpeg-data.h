/* jpeg-data.h
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

#ifndef __JPEG_DATA_H__
#define __JPEG_DATA_H__

#include <libjpeg/jpeg-marker.h>

#include <libexif/exif-data.h>
#include <libexif/exif-log.h>

typedef ExifData * JPEGContentAPP1;

typedef struct _JPEGContentGeneric JPEGContentGeneric;
struct _JPEGContentGeneric
{
	unsigned char *data;
	unsigned int size;
};

typedef union _JPEGContent JPEGContent;
union _JPEGContent
{
	JPEGContentGeneric generic;
	JPEGContentAPP1    app1;
};

typedef struct _JPEGSection JPEGSection;
struct _JPEGSection
{
	JPEGMarker marker;
	JPEGContent content;
};

typedef struct _JPEGData        JPEGData;
typedef struct _JPEGDataPrivate JPEGDataPrivate;

struct _JPEGData
{
	JPEGSection *sections;
	unsigned int count;

	unsigned char *data;
	unsigned int size;

	JPEGDataPrivate *priv;
};

JPEGData *jpeg_data_new           (void);
JPEGData *jpeg_data_new_from_file (const char *path);
JPEGData *jpeg_data_new_from_data (const unsigned char *data,
				   unsigned int size);

void      jpeg_data_ref   (JPEGData *data);
void      jpeg_data_unref (JPEGData *data);
void      jpeg_data_free  (JPEGData *data);

void      jpeg_data_load_data     (JPEGData *data, const unsigned char *d,
				   unsigned int size);
void      jpeg_data_save_data     (JPEGData *data, unsigned char **d,
				   unsigned int *size);

void      jpeg_data_load_file     (JPEGData *data, const char *path);
int       jpeg_data_save_file     (JPEGData *data, const char *path);

void      jpeg_data_set_exif_data (JPEGData *data, ExifData *exif_data);
ExifData *jpeg_data_get_exif_data (JPEGData *data);

void      jpeg_data_dump (JPEGData *data);

void      jpeg_data_append_section (JPEGData *data);

void      jpeg_data_log (JPEGData *data, ExifLog *log);

#endif /* __JPEG_DATA_H__ */
