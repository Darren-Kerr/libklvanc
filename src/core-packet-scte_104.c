/*
 * Copyright (c) 2016-2017 Kernel Labs Inc. All Rights Reserved
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

#include <libklvanc/vanc.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRINT_DEBUG_MEMBER_INT(m) printf(" %s = 0x%x\n", #m, m);

static const char *spliceInsertTypeName(unsigned char type)
{
	switch (type) {
	case 0x0000:                return "reserved";
	case SPLICESTART_NORMAL:    return "spliceStart_normal";
	case SPLICESTART_IMMEDIATE: return "spliceStart_immediate";
	case SPLICEEND_NORMAL:      return "spliceEnd_normal";
	case SPLICEEND_IMMEDIATE:   return "spliceEnd_immediate";
	case SPLICE_CANCEL:         return "splice_cancel";
	default:                    return "Undefined";
	}
}

static const char *som_operationName(unsigned short opID)
{
	if ((opID >= 0x8000) && (opID <= 0xbfff))
		return "User Defined";
	if ((opID >= 0x0013) && (opID <= 0x00ff))
		return "Reserved for future basic requests";
	switch (opID) {
	case 0x0000: return "general_response_data";
	case 0x0001: return "initial_request_data";
	case 0x0002: return "initial_response_data";
	case 0x0003: return "alive_request_data";
	case 0x0004: return "alive_response_data";
	case 0x0005:
	case 0x0006: return "User Defined";
	case 0x0007: return "inject_response_data";
	case 0x0008: return "inject_complete_response_data";
	case 0x0009: return "config_request_data";
	case 0x000a: return "config_response_data";
	case 0x000b: return "provisioning_request_data";
	case 0x000c: return "provisioning_response_data";
	case 0x000f: return "fault_request_data";
	case 0x0010: return "fault_response_data";
	case 0x0011: return "AS_alive_request_data";
	case 0x0012: return "AS_alive_response_data";
	default:     return "Reserved";
	}
}

static const char *mom_operationName(unsigned short opID)
{
	if ((opID >= 0xc000) && (opID <= 0xFFFE))
		return "User Defined";

	switch (opID) {
	case 0x0100: return "inject_section_data_request";
	case 0x0101: return "splice_data_request";
	case 0x0102: return "splice_null_request_data";
	case 0x0103: return "start_schedule_download_request_data";
	case 0x0104: return "time_signal_request_data";
	case 0x0105: return "transmit_schedule_request_data";
	case 0x0106: return "component_mode_DPI_request_data";
	case 0x0107: return "encrypted_DPI_request_data";
	case 0x0108: return "insert_descriptor_request_data";
	case 0x0109: return "insert_DTMF_descriptor_request_data";
	case 0x010a: return "insert_avail_descriptor_request_data";
	case 0x010b: return "insert_segmentation_descriptor_request_data";
	case 0x010c: return "proprietary_command_request_data";
	case 0x010d: return "schedule_component_mode_request_data";
	case 0x010e: return "schedule_definition_data_request";
	case 0x0300: return "delete_controlword_data_request";
	case 0x0301: return "update_controlword_data_request";
	default:     return "Reserved";
	}
}

static void hexdump(unsigned char *buf, unsigned int len, int bytesPerRow /* Typically 16 */, char *indent)
{
	printf("%s", indent);
	for (unsigned int i = 0; i < len; i++)
		printf("%02x%s", buf[i], ((i + 1) % bytesPerRow) ? " " : "\n%s");
	printf("\n");
}

static unsigned char *parse_splice_request_data(unsigned char *p, struct splice_request_data *d)
{
	d->splice_insert_type  = *(p++);
	d->splice_event_id     = *(p + 0) << 24 | *(p + 1) << 16 | *(p + 2) <<  8 | *(p + 3); p += 4;
	d->unique_program_id   = *(p + 0) << 8 | *(p + 1); p += 2;
	d->pre_roll_time       = *(p + 0) << 8 | *(p + 1); p += 2;
	d->brk_duration        = *(p + 0) << 8 | *(p + 1); p += 2;
	d->avail_num           = *(p++);
	d->avails_expected     = *(p++);
	d->auto_return_flag    = *(p++);

	/* TODO: We don't support splice cancel, but we'll pass it through with a warning. */
	switch (d->splice_insert_type) {
	case SPLICESTART_IMMEDIATE:
	case SPLICEEND_IMMEDIATE:
	case SPLICESTART_NORMAL:
	case SPLICEEND_NORMAL:
		break;
	default:
		/* We don't support this splice command */
		fprintf(stderr, "%s() splice_insert_type 0x%x [%s], error.\n", __func__,
			d->splice_insert_type,
		spliceInsertTypeName(d->splice_insert_type));
	}

	return p;
}

static unsigned char *parse_mom_timestamp(unsigned char *p, struct multiple_operation_message_timestamp *ts)
{
	ts->time_type = *(p++);
	switch (ts->time_type) {
	case 1:
		ts->time_type_1.UTC_seconds      = *(p + 0) << 24 | *(p + 1) << 16 | *(p + 2) << 8 | *(p + 3);
		ts->time_type_1.UTC_microseconds = *(p + 4) << 8 | *(p + 5);
		p += 6;
		break;
	case 2:
		ts->time_type_2.hours   = *(p + 0);
		ts->time_type_2.minutes = *(p + 1);
		ts->time_type_2.seconds = *(p + 2);
		ts->time_type_2.frames  = *(p + 3);
		p += 4;
		break;
	case 3:
		ts->time_type_3.GPI_number = *(p + 0);
		ts->time_type_3.GPI_edge   = *(p + 1);
		p += 2;
		break;
	case 0:
		/* The spec says no time is defined, this is a legitimate state. */
		break;
	default:
		fprintf(stderr, "%s() unsupported time_type 0x%x, assuming no time.\n", __func__, ts->time_type);
	}

	return p;
}

static int dump_mom(struct vanc_context_s *ctx, struct packet_scte_104_s *pkt)
{
	struct multiple_operation_message *m = &pkt->mo_msg;

	printf("SCTE104 multiple_operation_message struct\n");
	PRINT_DEBUG_MEMBER_INT(pkt->payloadDescriptorByte);

	PRINT_DEBUG_MEMBER_INT(m->rsvd);
	printf("    rsvd = %s\n", m->rsvd == 0xFFFF ? "Multiple_Ops (Reserved)" : "UNSUPPORTED");
	PRINT_DEBUG_MEMBER_INT(m->messageSize);
	PRINT_DEBUG_MEMBER_INT(m->protocol_version);
	PRINT_DEBUG_MEMBER_INT(m->AS_index);
	PRINT_DEBUG_MEMBER_INT(m->message_number);
	PRINT_DEBUG_MEMBER_INT(m->DPI_PID_index);
	PRINT_DEBUG_MEMBER_INT(m->SCTE35_protocol_version);
	PRINT_DEBUG_MEMBER_INT(m->num_ops);

	for (int i = 0; i < m->num_ops; i++) {
       		struct multiple_operation_message_operation *o = &m->ops[i];
		printf("\n opID[%d] = %s\n", i, mom_operationName(o->opID));
		PRINT_DEBUG_MEMBER_INT(o->opID);
		PRINT_DEBUG_MEMBER_INT(o->data_length);
		if (o->data_length)
			hexdump(o->data, o->data_length, 32, "    ");
		if (o->opID == MO_INIT_REQUEST_DATA) {
			struct splice_request_data *d = &pkt->sr_data;
			PRINT_DEBUG_MEMBER_INT(d->splice_insert_type);
			printf("    splice_insert_type = %s\n", spliceInsertTypeName(d->splice_insert_type));
			PRINT_DEBUG_MEMBER_INT(d->splice_event_id);
			PRINT_DEBUG_MEMBER_INT(d->unique_program_id);
			PRINT_DEBUG_MEMBER_INT(d->pre_roll_time);
			PRINT_DEBUG_MEMBER_INT(d->brk_duration);
			printf("    break_duration = %d (1/10th seconds)\n", d->brk_duration);
			PRINT_DEBUG_MEMBER_INT(d->avail_num);
			PRINT_DEBUG_MEMBER_INT(d->avails_expected);
			PRINT_DEBUG_MEMBER_INT(d->auto_return_flag);
		}
	}

	return KLAPI_OK;
}

static int dump_som(struct vanc_context_s *ctx, struct packet_scte_104_s *pkt)
{
        struct splice_request_data *d = &pkt->sr_data;
	struct single_operation_message *m = &pkt->so_msg;

	printf("SCTE104 single_operation_message struct\n");
	PRINT_DEBUG_MEMBER_INT(pkt->payloadDescriptorByte);

	PRINT_DEBUG_MEMBER_INT(m->opID);
	printf("   opID = %s\n", som_operationName(m->opID));
	PRINT_DEBUG_MEMBER_INT(m->messageSize);
	printf("   message_size = %d bytes\n", m->messageSize);
	PRINT_DEBUG_MEMBER_INT(m->result);
	PRINT_DEBUG_MEMBER_INT(m->result_extension);
	PRINT_DEBUG_MEMBER_INT(m->protocol_version);
	PRINT_DEBUG_MEMBER_INT(m->AS_index);
	PRINT_DEBUG_MEMBER_INT(m->message_number);
	PRINT_DEBUG_MEMBER_INT(m->DPI_PID_index);

	if (m->opID == SO_INIT_REQUEST_DATA) {
		PRINT_DEBUG_MEMBER_INT(d->splice_insert_type);
		printf("   splice_insert_type = %s\n", spliceInsertTypeName(d->splice_insert_type));
		PRINT_DEBUG_MEMBER_INT(d->splice_event_id);
		PRINT_DEBUG_MEMBER_INT(d->unique_program_id);
		PRINT_DEBUG_MEMBER_INT(d->pre_roll_time);
		PRINT_DEBUG_MEMBER_INT(d->brk_duration);
		printf("   break_duration = %d (1/10th seconds)\n", d->brk_duration);
		PRINT_DEBUG_MEMBER_INT(d->avail_num);
		PRINT_DEBUG_MEMBER_INT(d->avails_expected);
		PRINT_DEBUG_MEMBER_INT(d->auto_return_flag);
	} else
		printf("   unsupported m->opID = 0x%x\n", m->opID);

	for (int i = 0; i < pkt->payloadLengthBytes; i++)
		printf("%02x ", pkt->payload[i]);
	printf("\n");

	return KLAPI_OK;
}

int dump_SCTE_104(struct vanc_context_s *ctx, void *p)
{
	struct packet_scte_104_s *pkt = p;

	if (ctx->verbose)
		printf("%s() %p\n", __func__, (void *)pkt);

	if (pkt->so_msg.opID == SO_INIT_REQUEST_DATA)
		return dump_som(ctx, pkt);

	return dump_mom(ctx, pkt);
}

int parse_SCTE_104(struct vanc_context_s *ctx, struct packet_header_s *hdr, void **pp)
{
	if (ctx->verbose)
		printf("%s()\n", __func__);

	struct packet_scte_104_s *pkt = calloc(1, sizeof(*pkt));
	if (!pkt)
		return -ENOMEM;

	memcpy(&pkt->hdr, hdr, sizeof(*hdr));

        pkt->payloadDescriptorByte = hdr->payload[0];
        pkt->version               = (pkt->payloadDescriptorByte >> 3) & 0x03;
        pkt->continued_pkt         = (pkt->payloadDescriptorByte >> 2) & 0x01;
        pkt->following_pkt         = (pkt->payloadDescriptorByte >> 1) & 0x01;
        pkt->duplicate_msg         = pkt->payloadDescriptorByte & 0x01;

	/* We only support SCTE104 messages of type
	 * single_operation_message() that are completely
	 * self containined with a single VANC line, and
	 * are not continuation messages.
	 * Eg. payloadDescriptor value 0x08.
	 */
	if (pkt->payloadDescriptorByte != 0x08) {
		free(pkt);
		return -1;
	}

	/* First byte is the padloadDescriptor, the rest is the SCTE104 message...
	 * up to 200 bytes in length item 5.3.3 page 7 */
	for (int i = 0; i < 200; i++)
		pkt->payload[i] = hdr->payload[1 + i];

	struct single_operation_message *m = &pkt->so_msg;
	struct multiple_operation_message *mom = &pkt->mo_msg;

	/* Make sure we put the opID in the SOM reegardless of
	 * whether the message ius single or multiple.
	 * We rely on checking som.opID during the dump process
	 * to determinate the structure type.
	 */
	m->opID = pkt->payload[0] << 8 | pkt->payload[1];

	if (m->opID == SO_INIT_REQUEST_DATA) {

		/* TODO: Will we ever see a trigger in a SOM. Interal discussion says
		 *       no. We'll leave this active for the time being, pending removal.
		 */
		m->messageSize      = pkt->payload[2] << 8 | pkt->payload[3];
		m->result           = pkt->payload[4] << 8 | pkt->payload[5];
		m->result_extension = pkt->payload[6] << 8 | pkt->payload[7];
		m->protocol_version = pkt->payload[8];
		m->AS_index         = pkt->payload[9];
		m->message_number   = pkt->payload[10];
		m->DPI_PID_index    = pkt->payload[11] << 8 | pkt->payload[12];

		struct splice_request_data *d = &pkt->sr_data;

		d->splice_insert_type  = pkt->payload[13];
		d->splice_event_id     = pkt->payload[14] << 24 |
			pkt->payload[15] << 16 | pkt->payload[16] <<  8 | pkt->payload[17];
		d->unique_program_id   = pkt->payload[18] << 8 | pkt->payload[19];
		d->pre_roll_time       = pkt->payload[20] << 8 | pkt->payload[21];
		d->brk_duration        = pkt->payload[22] << 8 | pkt->payload[23];
		d->avail_num           = pkt->payload[24];
		d->avails_expected     = pkt->payload[25];
		d->auto_return_flag    = pkt->payload[26];

		/* TODO: We only support spliceStart_immediate and spliceEnd_immediate */
		switch (d->splice_insert_type) {
		case SPLICESTART_IMMEDIATE:
		case SPLICEEND_IMMEDIATE:
			break;
		default:
			/* We don't support this splice command */
			fprintf(stderr, "%s() splice_insert_type 0x%x, error.\n", __func__, d->splice_insert_type);
			free(pkt);
			return -1;
		}
	} else
	if (m->opID == 0xFFFF /* Multiple Operation Message */) {
		mom->rsvd                    = pkt->payload[0] << 8 | pkt->payload[1];
		mom->messageSize             = pkt->payload[2] << 8 | pkt->payload[3];
		mom->protocol_version        = pkt->payload[4];
		mom->AS_index                = pkt->payload[5];
		mom->message_number          = pkt->payload[6];
		mom->DPI_PID_index           = pkt->payload[7] << 8 | pkt->payload[8];
		mom->SCTE35_protocol_version = pkt->payload[9];

		unsigned char *p = &pkt->payload[10];
		p = parse_mom_timestamp(p, &mom->timestamp);
		
		mom->num_ops = *(p++);
		mom->ops = calloc(mom->num_ops, sizeof(struct multiple_operation_message_operation));
		if (!mom->ops) {
			fprintf(stderr, "%s() unable to allocate momo ram, error.\n", __func__);
			free(pkt);
			return -1;
		}

		for (int i = 0; i < mom->num_ops; i++) {
        		struct multiple_operation_message_operation *o = &mom->ops[i];
			o->opID = *(p + 0) << 8 | *(p + 1);
			o->data_length = *(p + 2) << 8 | *(p + 3);
			o->data = malloc(o->data_length);
			if (!o->data) {
				fprintf(stderr, "%s() Unable to allocate memory for mom op, error.\n", __func__);
			} else {
				memcpy(o->data, p + 4, o->data_length);
			}
			p += (4 + o->data_length);

			if (o->opID == MO_INIT_REQUEST_DATA)
				parse_splice_request_data(o->data, &pkt->sr_data);

#if 1
			printf("opID = 0x%04x [%s], length = 0x%04x : ", o->opID, mom_operationName(o->opID), o->data_length);
			hexdump(o->data, o->data_length, 32, "");
#endif
		}

		/* We'll parse this message but we'll only look for INIT_REQUEST_DATA
		 * sub messages, and construct a splice_request_data message.
		 * The rest of the message types will be ignored.
		 */
	}
	else {
		fprintf(stderr, "%s() Unsupported opID = %x, error.\n", __func__, m->opID);
		free(pkt);
		return -1;
	}

	if (ctx->callbacks && ctx->callbacks->scte_104)
		ctx->callbacks->scte_104(ctx->callback_context, ctx, pkt);

	*pp = pkt;
	return KLAPI_OK;
}

