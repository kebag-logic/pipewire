/* AVB support */
/* SPDX-FileCopyrightText: Copyright © 2025 Kebag-Logic */
/* SPDX-FileCopyrightText: Copyright © 2025 Alex Malki <alexandre.malki@kebag-logic.com> */
/* SPDX-FileCopyrightText: Copyright © 2025 Simon Gapp <simon.gapp@kebag-logic.com> */
/* SPDX-License-Identifier: MIT  */

#include <limits.h>

#include "../descriptors.h"
#include "../aecp-aem-state.h"
#include "../aecp-aem-descriptors.h"
#include "../aecp-aem-controls.h"

#include "aecp-aem-helpers.h"
#include "aecp-aem-types.h"
#include "aecp-aem-cmd-resp-common.h"
#include "aecp-aem-unsol-helper.h"


/**
 * TODO FUTURE: Make this descriptor more complex to handle all type of
 *  CONTROLS possible.
 */

/** For future use */
// const static unsigned int v_size_value[] = {
// 	[AECP_AEM_CTRL_LINEAR_INT8] = 1,
// 	[AECP_AEM_CTRL_LINEAR_UINT8] = 1,
// };


static int handle_unsol_control_common(struct aecp *aecp, struct descriptor *desc,
									bool has_expired)
{
	uint8_t buf[1024];
	void *m = buf;
	struct avb_ethernet_header *h = m;
	struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
	uint8_t value_desc, *value;
	struct avb_aem_desc_value_format *desc_formats;
	struct avb_aem_desc_control *ctrl_desc;
	struct aecp_aem_control_state *ctrl_state;
	struct avb_packet_aecp_aem_setget_control *control;
	uint64_t target_id = aecp->server->entity_id;

	size_t len = sizeof (*h) + sizeof(*p) + sizeof(*control) + sizeof (value_desc);
	int rc = 0;

	ctrl_state = (struct aecp_aem_control_state *) desc->ptr;
	ctrl_desc = &ctrl_state->desc;
	desc_formats = ctrl_desc->value_format;

	/** Reset the buffer */
	memset(buf, 0, sizeof(buf));
	/* Only support Milan so far */
	control = (struct avb_packet_aecp_aem_setget_control *) p->payload;
	control->descriptor_id = htons(desc->index);
	control->descriptor_type = htons(desc->type);
	p->aecp.target_guid = htobe64(target_id);
	ctrl_desc = (struct avb_aem_desc_control *) desc->ptr;
	desc_formats = (struct avb_aem_desc_value_format *) ctrl_desc->value_format;

	/** Only support identify so far */
	value = (uint8_t*)control->payload;
	if (has_expired) {
		desc_formats->current_value = 0;
	}

	value_desc = desc_formats->current_value;
	*value = value_desc;

	AVB_PACKET_AEM_SET_COMMAND_TYPE(p, AVB_AECP_AEM_CMD_SET_CONTROL);
	rc = reply_unsolicited_notifications(aecp, &ctrl_state->base.base_info,
				buf, len, has_expired);
	if (rc) {
		pw_log_error("Unsolicited notification failed \n");
		return rc;
	}

	return rc;
}

/**
 * @brief handle unsolicited notification for the set-control
 */
static int handle_unsol_set_control(struct aecp *aecp, struct descriptor *desc,
	uint64_t ctrler_id)
{
	int rc;
	/* Internal descriptor info */

	rc = handle_unsol_control_common(aecp, desc, false);
	if (rc) {
		spa_assert(0);
	}

	return rc;
}

/* IEEE 1722.1-2021, Sec. 7.4.25. SET_CONTROL Command*/
int handle_cmd_set_control(struct aecp *aecp, int64_t now, const void *m,
    int len)
{
	struct server *server = aecp->server;
	const struct avb_ethernet_header *h = m;
	const struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);

	/* Internals */
	/** For the control */
	struct descriptor *desc;
	struct avb_aem_desc_control *ctrl_desc;
	struct aecp_aem_control_state *ctrl_state;
	struct avb_aem_desc_value_format *desc_formats;
	struct avb_packet_aecp_aem_setget_control *control;
	uint16_t desc_type, desc_id, ctrler_id;
	uint8_t old_control_value;
	// Type of value for now is assumed to be uint8_t only Milan identify supported
	uint8_t *value_req;
	int rc;

	/* Value to calculate the position as defined in the IEEE 1722.1-2021, Sec. 7.3 */
    control = (struct avb_packet_aecp_aem_setget_control*)p->payload;
	desc_type = ntohs(control->descriptor_type);
	desc_id = ntohs(control->descriptor_id);
	ctrler_id = htobe64(p->aecp.controller_guid);

	desc = server_find_descriptor(server, desc_type, desc_id);
	if (desc == NULL)
		return reply_status(aecp, AVB_AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR, p, len);

	ctrl_state = (struct aecp_aem_control_state *) desc->ptr;
	ctrl_desc = &ctrl_state->desc;
	desc_formats = ctrl_desc->value_format;

	// Store old control value for success or fail response
	old_control_value = desc_formats->current_value;

	value_req = (uint8_t *)control->payload;
	// Now only support the Identify for Milan

	/* First case the value did not change */
	if (*value_req == desc_formats->current_value) {
		return reply_success(aecp, m, len);
	}

	/* Then verify if the step is fine*/
	if ((*value_req % desc_formats->step)) {
		return reply_set_control(aecp, m, len, AVB_AECP_AEM_STATUS_BAD_ARGUMENTS, old_control_value);
	}

	/** Then verify max */
	if ((*value_req > desc_formats->maximum)) {
		return reply_set_control(aecp, m, len, AVB_AECP_AEM_STATUS_BAD_ARGUMENTS, old_control_value);
	}

	/** Then verify min */
	if ((*value_req < desc_formats->minimum)) {
		return reply_set_control(aecp, m, len, AVB_AECP_AEM_STATUS_BAD_ARGUMENTS, old_control_value);
	}

	desc_formats->current_value = *value_req;

	/** Doing so will ask for unsolicited notifications */
	rc = aecp_aem_request_unsollicted_notifications(&ctrl_state->base, ctrler_id);
	if (rc) {
		spa_assert(0);
	}

    rc = reply_success(aecp, m, len);
	if (rc) {
		pw_log_error("Could not send the set-control response\n");
		return -1;
	}

	return handle_unsol_set_control(aecp, desc, ctrler_id);
}


/** TODO is this necessary for the IDENTIFY control ? */
int handle_unsol_timer_set_control(struct aecp *aecp, int64_t now)
{
	struct descriptor *desc;
	struct aecp_aem_control_state *ctrl_state;
	uint32_t desc_id;
	bool has_expired;
	int rc;

#define MAX_ID_DESC 65535

	for (desc_id = 0; desc_id < MAX_ID_DESC; desc_id++) {
		desc = server_find_descriptor(aecp->server, AVB_AEM_DESC_CONTROL, desc_id);
		if (desc == NULL) {
			break;
		}

		ctrl_state = (struct aecp_aem_control_state *) desc->ptr;

		has_expired = ctrl_state->base.base_info.expire_timeout < now;
		if (!has_expired) {
			continue;
		}

		rc = handle_unsol_control_common(aecp, desc, true);
		if (rc) {
			return rc;
		}

		ctrl_state->base.base_info.needs_update = false;
		if (has_expired) {
			ctrl_state->base.base_info.expire_timeout = LONG_MAX;
		}
	}

	return 0;
}