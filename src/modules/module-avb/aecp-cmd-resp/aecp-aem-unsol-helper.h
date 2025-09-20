/* AVB support */
/* SPDX-FileCopyrightText: Copyright © 2025 Kebag-Logic */
/* SPDX-FileCopyrightText: Copyright © 2025 Alex Malki <alexandre.malki@kebag-logic.com> */
/* SPDX-License-Identifier: MIT  */

#ifndef __AECP_AEM_UNSOL_HELPER_H__
#define __AECP_AEM_UNSOL_HELPER_H__

#include "../internal.h"

#include "../aecp-aem-state.h"
#include "../aecp-aem.h"
#include "../aecp.h"
#include "aecp-aem-types.h"

#define AECP_UNSOL_BUFFER_SIZE 		(128U)
#define AECP_AEM_MIN_PACKET_LENGTH 	AVB_PACKET_MIN_SIZE

static inline int reply_unsol_send(struct aecp *aecp, uint64_t controller_id,
	void *packet, size_t len, bool internal)
{
    uint16_t ctrler_index;
	int rc = 0;
	struct avb_ethernet_header *h;
	struct avb_packet_aecp_aem *p;
	struct aecp_aem_entity_state *entity_state;
	struct aecp_aem_unsol_notification_state *unsol_state;
	struct descriptor *desc;

	desc = server_find_descriptor(aecp->server, AVB_AEM_DESC_ENTITY, 0);
	if (desc == NULL) {
		pw_log_error("Could not find the ENTITY descriptor 0\n");
		return -1;
	}
	entity_state = (struct aecp_aem_entity_state *) desc->ptr;
	h = (struct avb_ethernet_header*) packet;
	p = SPA_PTROFF(h, sizeof(*h), void);

	unsol_state = entity_state->unsol_notif_state;
		// Loop through all the unsol entities.
	for (ctrler_index = 0;
			ctrler_index < ARRAY_SIZE(entity_state->unsol_notif_state);
		 	ctrler_index++)
	{
		if (!unsol_state[ctrler_index].is_registered) {
			pw_log_info("Not registered %d\n", ctrler_index);
			continue;
		}

		if ((controller_id == unsol_state[ctrler_index].ctrler_endity_id) && !internal) {
			/* Do not send unsolicited if that the one creating the udpate, and
				this is not a timeout.*/
			pw_log_info("Do not send twice of %lx %lx\n", controller_id,
				unsol_state[ctrler_index].ctrler_endity_id );
			continue;
		}

		p->aecp.controller_guid = htobe64(unsol_state[ctrler_index].ctrler_endity_id);
		p->aecp.sequence_id = htons(unsol_state[ctrler_index].next_seq_id);

		unsol_state[ctrler_index].next_seq_id++;
		pw_log_warn(" pointer is %p", &unsol_state[ctrler_index]);
		rc = avb_server_send_packet(aecp->server, unsol_state[ctrler_index].ctrler_mac_addr,
                AVB_TSN_ETH, packet, len);

		if (rc) {
			pw_log_error("while sending packet to %lx\n", unsol_state[ctrler_index].ctrler_endity_id);
			return rc;
		}
	}

	return rc;
}

static inline void reply_unsol_notifications_prepare(struct aecp *aecp,
	uint8_t *buf, void *packet, size_t len)
{
	struct avb_ethernet_header *h;
	struct avb_packet_aecp_aem *p;
	size_t ctrl_data_length;

		/* Here the value of 12 is the delta between the target_entity_id and
		start of the AECP message specific data */
	ctrl_data_length = len - (sizeof(*h) + sizeof(*p)) + 12;

	h = (struct avb_ethernet_header*) packet;
	p = SPA_PTROFF(h, sizeof(*h), void);

    p->aecp.hdr.subtype = AVB_SUBTYPE_AECP;
	AVB_PACKET_AECP_SET_MESSAGE_TYPE(&p->aecp, AVB_AECP_MESSAGE_TYPE_AEM_RESPONSE);
	AVB_PACKET_SET_VERSION(&p->aecp.hdr, 0);
	AVB_PACKET_AECP_SET_STATUS(&p->aecp, AVB_AECP_AEM_STATUS_SUCCESS);
	AVB_PACKET_SET_LENGTH(&p->aecp.hdr, ctrl_data_length);
	p->u = 1;
	p->aecp.target_guid = htobe64(aecp->server->entity_id);
}

static inline int reply_unsolicited_notifications_ctrler_id(struct aecp *aecp,
	uint64_t controller_entity_id, void *packet, size_t len,
	 bool internal)
{
	uint8_t buf[AECP_UNSOL_BUFFER_SIZE];

	if (len < AECP_AEM_MIN_PACKET_LENGTH) {
		memset(buf, 0, AECP_AEM_MIN_PACKET_LENGTH);
		memcpy(buf, packet, len);
		len = AECP_AEM_MIN_PACKET_LENGTH;
		packet = buf;
	}

	/** Retrieve the entity descriptor */
	reply_unsol_notifications_prepare(aecp, buf, packet, len);

    return reply_unsol_send(aecp, controller_entity_id, packet, len, internal);
}

/**
 * @brief Sends unsolicited notifications. Does not sends information unless to
 *  the controller id unless an internal change has happenned (timeout, action
 *  etc)
 *
 */
static inline int reply_unsolicited_notifications(struct aecp *aecp,
	struct aecp_aem_base_info *b_state, void *packet, size_t len,
	 bool internal)
{
	uint8_t buf[AECP_UNSOL_BUFFER_SIZE];

	if (len < AECP_AEM_MIN_PACKET_LENGTH) {
		memset(buf, 0, AECP_AEM_MIN_PACKET_LENGTH);
		memcpy(buf, packet, len);
		len = AECP_AEM_MIN_PACKET_LENGTH;
		packet = buf;
	}

	/** Retrieve the entity descriptor */
	reply_unsol_notifications_prepare(aecp, buf, packet, len);

    return reply_unsol_send(aecp, b_state->controller_entity_id, packet, len,
							 internal);
}

#endif //__AECP_AEM_UNSOL_HELPER_H__