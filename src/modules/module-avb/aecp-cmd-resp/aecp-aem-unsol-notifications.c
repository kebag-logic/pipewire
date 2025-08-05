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
	struct aecp_aem_unsol_notification_state unsol = {0};

	uint64_t controller_id = htobe64(p->aecp.controller_guid);
	uint64_t target_id = htobe64(p->aecp.target_guid);
	uint16_t index;
	int rc;

		return reply_not_implemented(aecp, m, len);
}

int handle_cmd_deregister_unsol_notifications(struct aecp *aecp,
	 int64_t now, const void *m, int len)
{
	const struct avb_ethernet_header *h = m;
	const struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
	struct aecp_aem_unsol_notification_state unsol = {0};

	uint64_t controller_id = htobe64(p->aecp.controller_guid);
	uint64_t target_id = htobe64(p->aecp.target_guid);
	uint16_t index;
	int rc;


		return reply_not_implemented(aecp, m, len);
}
