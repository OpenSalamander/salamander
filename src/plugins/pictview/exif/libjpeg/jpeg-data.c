/* jpeg-data.c
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
#include "jpeg-data.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <crtdbg.h>

#include <libexif/i18n.h>

#pragma warning(push)
#pragma warning(disable: 4267) // FIXME_X64 - docasne potlacen warning, vyresit

struct _JPEGDataPrivate
{
	unsigned int ref_count;

	ExifLog *log;
};

JPEGData *
jpeg_data_new (void)
{
	JPEGData *data;

	data = malloc (sizeof (JPEGData));
	if (!data)
		return (NULL);
	memset (data, 0, sizeof (JPEGData));
	data->priv = malloc (sizeof (JPEGDataPrivate));
	if (!data->priv) {
		free (data);
		return (NULL);
	}
	memset (data->priv, 0, sizeof (JPEGDataPrivate));
	data->priv->ref_count = 1;

	return (data);
}

void
jpeg_data_append_section (JPEGData *data)
{
	JPEGSection *s;

	if (!data) return;

	if (!data->count)
		s = malloc (sizeof (JPEGSection));
	else
		s = realloc (data->sections,
			     sizeof (JPEGSection) * (data->count + 1));
	if (!s) {
		EXIF_LOG_NO_MEMORY (data->priv->log, "jpeg-data", 
				sizeof (JPEGSection) * (data->count + 1));
		return;
	}
	memset(s + data->count, 0, sizeof (JPEGSection));
	data->sections = s;
	data->count++;
}

/* jpeg_data_save_file returns 1 on succes, 0 on failure */
int
jpeg_data_save_file (JPEGData *data, const char *path)
{
	FILE *f;
	unsigned char *d = NULL;
	unsigned int size = 0, written;

	jpeg_data_save_data (data, &d, &size);
	if (!d)
		return 0;

	remove (path);
	f = fopen (path, "wb");
	if (!f) {
		free (d);
		return 0;
	}
	written = fwrite (d, 1, size, f);
	fclose (f);
	free (d);
	if (written == size)  {
		return 1;
	}
	remove(path);
	return 0;
}

void
jpeg_data_save_data (JPEGData *data, unsigned char **d, unsigned int *ds)
{
	unsigned int i, eds = 0;
	JPEGSection s;
	unsigned char *ed = NULL;

	if (!data)
		return;
	if (!d)
		return;
	if (!ds)
		return;

	for (*ds = i = 0; i < data->count; i++) {
		s = data->sections[i];

		/* Write the marker */
		*d = realloc (*d, sizeof (char) * (*ds + 2));
		(*d)[*ds + 0] = 0xff;
		(*d)[*ds + 1] = s.marker;
		*ds += 2;

		switch (s.marker) {
		case JPEG_MARKER_SOI:
		case JPEG_MARKER_EOI:
			break;
		case JPEG_MARKER_APP1:
			exif_data_save_data (s.content.app1, &ed, &eds);
			if (!ed) break;
			*d = realloc (*d, sizeof (char) * (*ds + 2));
			(*d)[*ds + 0] = (eds + 2) >> 8;
			(*d)[*ds + 1] = (eds + 2) >> 0;
			*ds += 2;
			*d = realloc (*d, sizeof (char) * (*ds + eds));
			memcpy (*d + *ds, ed, eds);
			*ds += eds;
			free (ed);
			break;
		default:
			*d = realloc (*d, sizeof (char) *
					(*ds + s.content.generic.size + 2));
			(*d)[*ds + 0] = (s.content.generic.size + 2) >> 8;
			(*d)[*ds + 1] = (s.content.generic.size + 2) >> 0;
			*ds += 2;
			memcpy (*d + *ds, s.content.generic.data,
				s.content.generic.size);
			*ds += s.content.generic.size;

			/* In case of SOS, we need to write the data. */
			if (s.marker == JPEG_MARKER_SOS) {
				*d = realloc (*d, *ds + data->size);
				memcpy (*d + *ds, data->data, data->size);
				*ds += data->size;
			}
			break;
		}
	}
}

JPEGData *
jpeg_data_new_from_data (const unsigned char *d,
			 unsigned int size)
{
	JPEGData *data;

	data = jpeg_data_new ();
	jpeg_data_load_data (data, d, size);
	return (data);
}

void
jpeg_data_load_data (JPEGData *data, const unsigned char *d,
		     unsigned int size)
{
	unsigned int i, o, len;
	JPEGSection *s;
	JPEGMarker marker;

	if (!data) return;
	if (!d) return;

	for (o = 0; o < size;) {

		/*
		 * JPEG sections start with 0xff. The first byte that is
		 * not 0xff is a marker (hopefully).
		 */
		for (i = 0; i < MIN(7, size - o); i++)
			if (d[o + i] != 0xff)
				break;
		if (!JPEG_IS_MARKER (d[o + i])) {
			exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA, "jpeg-data",
					_("Data does not follow JPEG specification."));
			return;
		}
		marker = d[o + i];

		/* Append this section */
		jpeg_data_append_section (data);
		if (!data->count) return;
		s = &data->sections[data->count - 1];
		s->marker = marker;
		o += i + 1;

		switch (s->marker) {
		case JPEG_MARKER_SOI:
		case JPEG_MARKER_EOI:
			break;
		default:

			/* Read the length of the section */
			len = ((d[o] << 8) | d[o + 1]) - 2;
			if (len > size) { o = size; break; }
			o += 2;
			if (o + len > size) { o = size; break; }

			switch (s->marker) {
			case JPEG_MARKER_APP1:
				s->content.app1 = exif_data_new_from_data (
							d + o - 4, len + 4);
				break;
			default:
				s->content.generic.size = len;
				s->content.generic.data =
						malloc (sizeof (char) * len);
				memcpy (s->content.generic.data, &d[o], len);

				/* In case of SOS, image data will follow. */
				if (s->marker == JPEG_MARKER_SOS) {
					/* -2 means 'take all but the last 2 bytes which are hoped to be JPEG_MARKER_EOI */
					data->size = size - 2 - o - len;
					if (d[o + len + data->size] != 0xFF) {
						/* A truncated file (i.e. w/o JPEG_MARKER_EOI at the end).
						   Instead of trying to use the last two bytes as marker,
						   touching memory beyond allocated memory and posssibly saving
						   back screwed file, we rather take the rest of the file. */
						data->size += 2;
					}
					data->data = malloc (
						sizeof (char) * data->size);
					memcpy (data->data, d + o + len,
						data->size);
					o += data->size;
				}
				break;
			}
			o += len;
			break;
		}
	}
}

JPEGData *
jpeg_data_new_from_file (const char *path)
{
	JPEGData *data;

	data = jpeg_data_new ();
	jpeg_data_load_file (data, path);
	return (data);
}

void
jpeg_data_load_file (JPEGData *data, const char *path)
{
	FILE *f;
	unsigned char *d;
	unsigned int size;

	if (!data) return;
	if (!path) return;

	f = fopen (path, "rb");
	if (!f) {
		exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA, "jpeg-data",
				_("Path '%s' invalid."), path);
		return;
	}

	/* For now, we read the data into memory. Patches welcome... */
	fseek (f, 0, SEEK_END);
	size = ftell (f);
	fseek (f, 0, SEEK_SET);
	d = malloc (size);
	if (!d) {
		EXIF_LOG_NO_MEMORY (data->priv->log, "jpeg-data", size);
		fclose (f);
		return;
	}
	if (fread (d, 1, size, f) != size) {
		free (d);
		fclose (f);
		exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA, "jpeg-data",
				_("Could not read '%s'."), path);
		return;
	}
	fclose (f);

	jpeg_data_load_data (data, d, size);
	free (d);
}

void
jpeg_data_ref (JPEGData *data)
{
	if (!data)
		return;

	data->priv->ref_count++;
}

void
jpeg_data_unref (JPEGData *data)
{
	if (!data)
		return;

	if (data->priv) {
		data->priv->ref_count--;
		if (!data->priv->ref_count)
			jpeg_data_free (data);
	}
}

void
jpeg_data_free (JPEGData *data)
{
	unsigned int i;
	JPEGSection s;

	if (!data)
		return;

	if (data->count) {
		for (i = 0; i < data->count; i++) {
			s = data->sections[i];
			switch (s.marker) {
			case JPEG_MARKER_SOI:
			case JPEG_MARKER_EOI:
				break;
			case JPEG_MARKER_APP1:
				exif_data_unref (s.content.app1);
				break;
			default:
				free (s.content.generic.data);
				break;
			}
		}
		free (data->sections);
	}

	if (data->data)
		free (data->data);

	if (data->priv) {
		if (data->priv->log) {
			exif_log_unref (data->priv->log);
			data->priv->log = NULL;
		}
		free (data->priv);
	}

	free (data);
}

void
jpeg_data_dump (JPEGData *data)
{
	unsigned int i;
	JPEGContent content;
	JPEGMarker marker;

	if (!data)
		return;

	printf ("Dumping JPEG data (%i bytes of data)...\n", data->size);
	for (i = 0; i < data->count; i++) {
		marker = data->sections[i].marker;
		content = data->sections[i].content;
		printf ("Section %i (marker 0x%x - %s):\n", i, marker,
			jpeg_marker_get_name (marker));
		printf ("  Description: %s\n",
			jpeg_marker_get_description (marker));
		switch (marker) {
                case JPEG_MARKER_SOI:
                case JPEG_MARKER_EOI:
			break;
                case JPEG_MARKER_APP1:
			exif_data_dump (content.app1);
			break;
                default:
			printf ("  Size: %i\n", content.generic.size);
                        printf ("  Unknown content.\n");
                        break;
                }
        }
}

static JPEGSection *
jpeg_data_get_section (JPEGData *data, JPEGMarker marker)
{
	unsigned int i;

	if (!data)
		return (NULL);

	for (i = 0; i < data->count; i++)
		if (data->sections[i].marker == marker)
			return (&data->sections[i]);
	return (NULL);
}

ExifData *
jpeg_data_get_exif_data (JPEGData *data)
{
	JPEGSection *section;

	if (!data)
		return NULL;

	section = jpeg_data_get_section (data, JPEG_MARKER_APP1);
	if (section) {
		exif_data_ref (section->content.app1);
		return (section->content.app1);
	}

	return (NULL);
}

void
jpeg_data_set_exif_data (JPEGData *data, ExifData *exif_data)
{
	JPEGSection *section;

	if (!data) return;

	section = jpeg_data_get_section (data, JPEG_MARKER_APP1);
	if (!section) {
		jpeg_data_append_section (data);
		if (data->count < 2) return;
		memmove (&data->sections[2], &data->sections[1],
			 sizeof (JPEGSection) * (data->count - 2));
		section = &data->sections[1];
	} else {
		exif_data_unref (section->content.app1);
	}
	section->marker = JPEG_MARKER_APP1;
	section->content.app1 = exif_data;
	exif_data_ref (exif_data);
}

void
jpeg_data_log (JPEGData *data, ExifLog *log)
{
	if (!data || !data->priv) return;
	if (data->priv->log) exif_log_unref (data->priv->log);
	data->priv->log = log;
	exif_log_ref (log);
}

#pragma warning(pop) // FIXME_X64 - docasne potlacen warning, vyresit
