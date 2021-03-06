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

/**
 * @file	vanc-payload_information.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2016 Kernel Labs Inc. All Rights Reserved.
 * @brief	TODO - Brief description goes here.
 */

#ifndef _VANC_PAYLOAD_INFORMATION_H
#define _VANC_PAYLOAD_INFORMATION_H

#include <libklvanc/vanc-packets.h>

#ifdef __cplusplus
extern "C" {
#endif  

/**
 * @brief	TODO - Brief description goes here.
 */
enum payload_aspect_ratio_e
{
	ASPECT_UNDEFINED = 0,
	ASPECT_4x3,
	ASPECT_16x9,
};

/**
 * @brief	TODO - Brief description goes here.
 */
enum payload_afd_e
{
	AFD_UNDEFINED = 0,
	AFD_BOX_16x9_TOP,
	AFD_BOX_14x9_TOP,
	AFD_BOX_16x9_CENTER,
	AFD_FULL_FRAME,
	AFD_16x9_CENTER,
	AFD_14x9_CENTER,
	AFD_4x3_WITH_ALTERNATIVE_14x9_CENTER,
	AFD_16x9_WITH_ALTERNATIVE_14x9_CENTER,
	AFD_16x9_WITH_ALTERNATIVE_4x3_CENTER,
};

/**
 * @brief	TODO - Brief description goes here.
 */
struct packet_payload_information_s
{
	struct packet_header_s hdr;
	enum payload_aspect_ratio_e aspectRatio;
	enum payload_afd_e afd;
	unsigned short barDataValue[2];
	unsigned char barDataFlags;
};

/**
 * @brief	TODO - Brief description goes here.
 * @param[in]	enum payload_afd_e afd - Brief description goes here.
 * @return	Success - User facing printable string.
 * @return	Error - NULL
 */
const char *afd_to_string(enum payload_afd_e afd);

/**
 * @brief	TODO - Brief description goes here.
 * @param[in]	enum payload_aspect_ratio_e ar - Brief description goes here.
 * @return	Success - User facing printable string.
 * @return	Error - NULL
 */
const char *aspectRatio_to_string(enum payload_aspect_ratio_e ar);

/**
 * @brief	TODO - Brief description goes here.
 * @param[in]	struct vanc_context_s *ctx, void *p - Brief description goes here.
 * @return	0 - Success
 * @return	< 0 - Error
 */
int dump_PAYLOAD_INFORMATION(struct vanc_context_s *ctx, void *p);

#ifdef __cplusplus
};
#endif  

#endif /* _VANC_PAYLOAD_INFORMATION_H */
