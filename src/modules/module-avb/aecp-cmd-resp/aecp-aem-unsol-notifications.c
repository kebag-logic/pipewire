/* AVB support */
/* SPDX-FileCopyrightText: Copyright © 2025 Kebag-Logic */
/* SPDX-FileCopyrightText: Copyright © 2025 Alex Malki <alexandre.malki@kebag-logic.com> */
/* SPDX-License-Identifier: MIT  */

#include "../aecp-aem-state.h"
#include "../descriptors.h"

#include "aecp-aem-types.h"
#include "aecp-aem-unsol-notifications.h"
#include "aecp-aem-helpers.h"

/** Registration of unsolicited notifications */

int handle_cmd_register_unsol_notifications(struct aecp *aecp, int64_t now,
	 const void *m, int len)
{
	const struct avb_ethernet_header *h = m;
	const struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
	struct descriptor *desc;
	struct aecp_aem_entity_state *entity_state;
	struct aecp_aem_unsol_notification_state *unsol;
	uint64_t controller_id = htobe64(p->aecp.controller_guid);
	uint16_t index;

	desc = server_find_descriptor(aecp->server, AVB_AEM_DESC_ENTITY, 0);
	if (desc == NULL)
		return reply_status(aecp, AVB_AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR, p, len);

	entity_state = (struct aecp_aem_entity_state*) desc->ptr;
	unsol = entity_state->unsol_notif_state;

#ifdef USE_MILAN
	for (index = 0; index < AECP_AEM_UNSOL_NOTIFICATION_REG_CONTROLLER_MAX;
				index++)  {

		if ((unsol[index].ctrler_endity_id == controller_id) &&
				unsol[index].is_registered) {
			pw_log_warn("controller 0x%lx, already registered\n", controller_id);
			return reply_success(aecp, m, len);
		}
	}

	for (index = 0; index < AECP_AEM_UNSOL_NOTIFICATION_REG_CONTROLLER_MAX;
				index++)  {

		if (!unsol[index].is_registered) {
			break;
		}
	}

	if (index == AECP_AEM_UNSOL_NOTIFICATION_REG_CONTROLLER_MAX) {
		return reply_no_resources(aecp, m, len);
	}

	unsol[index].ctrler_endity_id = controller_id;
	memcpy(&unsol[index].ctrler_mac_addr, h->src, sizeof(h->src));
	unsol[index].is_registered = true;
	unsol[index].port_id = 0;
	unsol[index].next_seq_id = 0;

	pw_log_warn("Unsolicited notification registration for 0x%lx", controller_id);
	pw_log_warn(" pointer is %p", &unsol[index]);
	return reply_success(aecp, m, len);
#else
		return reply_not_implemented(aecp, m, len);
#endif //USE_MILAN
}

int handle_cmd_deregister_unsol_notifications(struct aecp *aecp,
	 int64_t now, const void *m, int len)
{
	const struct avb_ethernet_header *h = m;
	const struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
	struct descriptor *desc;
	struct aecp_aem_entity_state *entity_state;
	struct aecp_aem_unsol_notification_state *unsol;

	uint64_t controller_id = htobe64(p->aecp.controller_guid);
	uint16_t index;

	desc = server_find_descriptor(aecp->server, AVB_AEM_DESC_ENTITY, 0);
	if (desc == NULL)
		return reply_status(aecp, AVB_AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR, p, len);

	entity_state = (struct aecp_aem_entity_state*) desc->ptr;
	unsol = entity_state->unsol_notif_state;


	#ifdef USE_MILAN
	// Check the list if registered
	for (index = 0; index < AECP_AEM_UNSOL_NOTIFICATION_REG_CONTROLLER_MAX;
				index++)  {

		if ((unsol[index].ctrler_endity_id == controller_id) &&
				unsol[index].is_registered) {
			break;
		}
	}

	// Never made it to the list
	if (index == AECP_AEM_UNSOL_NOTIFICATION_REG_CONTROLLER_MAX) {
		pw_log_warn("Controller %lx never made it the registrered list\n",
					 controller_id);
		return reply_success(aecp, m, len);
	}

	unsol[index].ctrler_endity_id = 0;
	memset(&unsol[index].ctrler_mac_addr, 0, sizeof(unsol[index].ctrler_mac_addr));
	unsol[index].is_registered = false;
	unsol[index].port_id = 0;
	unsol[index].next_seq_id = 0;

	pw_log_info("unsol de-registration for 0x%lx at idx=%d", controller_id, index);

	return reply_success(aecp, m, len);
#else
		return reply_not_implemented(aecp, m, len);
#endif //USE_MILAN
}
