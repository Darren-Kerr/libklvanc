/*
 * Copyright (c) 2016 Kernel Labs Inc. All Rights Reserved
 *
 * Address: Kernel Labs Inc., PO Box 745, St James, NY. 11780
 * Contact: sales@kernellabs.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <libklvanc/vanc-lines.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct vanc_line_s *vanc_line_create(int line_number)
{
	struct vanc_line_s *new_line = NULL;
	new_line = (struct vanc_line_s *)malloc(sizeof(struct vanc_line_s));
	if (new_line == NULL)
		return NULL;
	memset(new_line, 0, sizeof(struct vanc_line_s));
	new_line->line_number = line_number;
	return new_line;
}

void vanc_line_free(struct vanc_line_s *line)
{
	for (int i = 0; i < MAX_VANC_ENTRIES; i++) {
		if (line->p_entries[i] != NULL) {
			free(line->p_entries[i]->payload);
			free(line->p_entries[i]);
		}
	}
	free(line);
}

int vanc_line_insert(struct vanc_line_set_s *vanc_lines, uint16_t * pixels,
		     int pixel_width, int line_number, int horizontal_offset)
{
	int i;
	struct vanc_line_s *line = vanc_lines->lines[0];
	struct vanc_entry_s *new_entry =
	    (struct vanc_entry_s *)malloc(sizeof(struct vanc_entry_s));
	if (new_entry == NULL)
		return -ENOMEM;

	new_entry->payload =
	    (uint16_t *) malloc(pixel_width * sizeof(uint16_t));
	if (new_entry->payload == NULL) {
		free(new_entry);
		return -ENOMEM;
	}
	memcpy(new_entry->payload, pixels, pixel_width * sizeof(uint16_t));
	new_entry->h_offset = horizontal_offset;
	new_entry->pixel_width = pixel_width;

	/* See if there is already a line allocated for the target line, if not, create one */
	for (i = 0; i < MAX_VANC_LINES; i++) {
		if (vanc_lines->lines[i] == NULL) {
			line = vanc_line_create(line_number);
			vanc_lines->lines[i] = line;
			vanc_lines->num_lines++;
			break;
		}
		if (vanc_lines->lines[i]->line_number == line_number) {
			line = vanc_lines->lines[i];
			break;
		}
	}

	if (i == MAX_VANC_LINES) {
		/* Array is full */
		fprintf(stderr, "array of lines is full!\n");
		free(new_entry->payload);
		free(new_entry);
		return -ENOMEM;
	}

	/* Now insert the VANC entry into the line */
	if (line->num_entries == MAX_VANC_ENTRIES) {
		/* Array is full */
		fprintf(stderr, "line is full!\n");
		free(new_entry->payload);
		free(new_entry);
		return -ENOMEM;
	}

	line->p_entries[line->num_entries++] = new_entry;
	return 0;
}

static int vanc_ent_comp(const void *a, const void *b)
{
	const struct vanc_entry_s *vala = (struct vanc_entry_s *)a;
	const struct vanc_entry_s *valb = (struct vanc_entry_s *)b;

	if (vala->h_offset < valb->h_offset)
		return -1;

	if (vala->h_offset == valb->h_offset)
		return 0;
	return 1;
}

int generate_vanc_line(struct vanc_line_s *line, uint16_t ** outbuf,
		       int *out_len, int line_pixel_width)
{
	int pixels_used = 0;
	int i;

	/* Sort the VANC entries on the line prior to processing */
	qsort(line->p_entries, line->num_entries, sizeof(struct vanc_entry_s *),
	      vanc_ent_comp);

	/* Now iterate through and adjust any offsets to prevent overlap.  Also calculate
	   how big a buffer we need for the final array of pixels */
	for (i = 0; i < line->num_entries; i++) {
		struct vanc_entry_s *entry = line->p_entries[i];
		if (entry->h_offset < pixels_used) {
			/* New entry would overwrite earlier entry, so push it out */
			entry->h_offset = pixels_used;
		} else if (entry->h_offset > pixels_used) {
			/* Don't let there be a gap between the latest entry and the
			   previous entry.  VANC entries must be contiguous
			   (see SMPTE ST291-1-2011 Sec 7.3) */
			entry->h_offset = pixels_used;
		}

		/* Check all bytes after VANC start words for illegal values */
		for (int j = 3; j < entry->pixel_width; j++) {
			if (entry->payload[j] <= 0x0003
			    || entry->payload[j] >= 0x03FC) {
				fprintf(stderr,
					"VANC line %d has entry with illegal payload at offset %d. Skipping.  offset=%d len=%d",
					line->line_number, j, entry->h_offset,
					entry->pixel_width);
				for (int k = 0; k < entry->pixel_width; k++) {
					fprintf(stderr, "%04x ",
						entry->payload[k]);
				}
				fprintf(stderr, "\n");
				entry->pixel_width = 0;
				break;
			}
		}

		/* Don't let sum of all VANC entries overflow end of line */
		if ((entry->h_offset + entry->pixel_width) > line_pixel_width) {
			/* Set the length to zero so this entry gets skipped */
			fprintf(stderr,
				"VANC line %d would overflow thus skipping.  offset=%d len=%d",
				line->line_number, entry->h_offset,
				entry->pixel_width);
			entry->pixel_width = 0;
			continue;
		}

		pixels_used += entry->pixel_width;
	}

	*outbuf = (uint16_t *) malloc(pixels_used * sizeof(uint16_t));
	if (*outbuf == NULL) {
		return -ENOMEM;
	}
	*out_len = pixels_used;

	/* Assemble the final line ensuring there are no discontinuities between
	   the VANC entries */
	for (i = 0; i < line->num_entries; i++) {
		struct vanc_entry_s *entry = line->p_entries[i];
		memcpy((*outbuf) + entry->h_offset, entry->payload,
		       entry->pixel_width * (sizeof(uint16_t)));
	}
	return 0;
}
