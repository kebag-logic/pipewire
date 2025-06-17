/* AVB support */
/* SPDX-FileCopyrightText: Copyright © 2025 Kebag-Logic */
/* SPDX-FileCopyrightText: Copyright © 2025 Alex Malki <alexandre.malki@kebag-logic.com> */
/* SPDX-FileCopyrightText: Copyright © 2025 Simon Gapp <simon.gapp@kebag-logic.com> */
/* SPDX-License-Identifier: MIT  */

#include "../aecp-aem-state.h"
#include "../descriptors.h"
#include "aecp-aem-helpers.h"
#include "aecp-aem-unsol-helper.h"
#include "aecp-aem-types.h"

#include "aecp-aem-cmd-set-name.h"
#include "aecp-aem-name-common.h"

// static int request_unsolicited_notification(struct aecp *aecp,
//     struct descriptor *desc, uint64_t ctrler_id, uint16_t name_index,
//     uint16_t config_index)
// {
//     int rc;

//     name_state.base_desc.desc = desc;
//     name_state.name_index = name_index;
//     name_state.base_desc.config_index = config_index;

//     return rc;
// }

static int handle_set_name_entity(struct descriptor *desc, uint16_t str_idex,
     const char *new_name)
{
    char *dest;

    if (aem_aecp_get_name_entity(desc, str_idex, &dest)) {
        spa_assert(0);
    }

    memcpy(dest, new_name, AECP_AEM_STRLEN_MAX);
    return 0;
}

static int handle_set_name_generic(struct descriptor *desc, uint16_t str_idex,
    const char *new_name)
{
    // This works beause the aem descriptors all starts with the group name
    char *dest = (char *)desc->ptr;
    memcpy(dest, new_name, AECP_AEM_STRLEN_MAX);
    return 0;
}


static int handle_unsol_set_name(struct aecp *aecp, struct descriptor *desc,
    struct avb_packet_aecp_aem_setget_name *sgname_rcved, uint64_t ctrler_id)
{
    uint8_t buf[512];
    size_t len;
    const char *src_name = NULL;
	struct avb_ethernet_header *h = (struct avb_ethernet_header *) buf;
	struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
	struct avb_packet_aecp_aem_setget_name *sg_name =
                        (struct avb_packet_aecp_aem_setget_name *) p->payload;

	/** Setup the packet for the unsolicited notification*/
	AVB_PACKET_AEM_SET_COMMAND_TYPE(p, AVB_AECP_AEM_CMD_SET_NAME);
	p->u = 1;
	p->aecp.target_guid = htobe64(aecp->server->entity_id);

    // Name update needs to differentiate between Entity Name, Group Name and everything else
    if (desc->type == AVB_AEM_DESC_ENTITY) {
        struct avb_aem_desc_entity *entity = (struct avb_aem_desc_entity *) desc->ptr;

        if (ntohs(sgname_rcved->name_index) == AECP_AEM_NAME_INDEX_ENTITY_ITSELF) {
            src_name = entity->entity_name;
        } else if (ntohs(sgname_rcved->name_index) == AECP_AEM_NAME_INDEX_ENTITY_GROUP) {
            src_name = entity->group_name;
        } else {
            pw_log_error("Invalid name index for entity descriptor in "
                          "unsolicited notification: %d",
                          ntohs(sgname_rcved->name_index));
            return -1;
        }
    } else {
        // Default to the start of the struct for all other descriptors
        src_name = (char*) desc->ptr;
    }

    memcpy(sg_name->name, src_name, sizeof(sg_name->name));

    sg_name->descriptor_index = htons(desc->index);
    sg_name->descriptor_type = htons(desc->type);
    sg_name->configuration_index = sgname_rcved->configuration_index;
    sg_name->name_index = sgname_rcved->name_index;

    len = sizeof(*p) + sizeof(*sg_name) + sizeof(*h);

    return reply_unsolicited_notifications_ctrler_id(aecp, ctrler_id, buf, len, false);
}

// TODO PERSISTENCE: Handle an overlay.
/** IEEE 1722.1-2021, Sec. 7.4.17 */
int handle_cmd_set_name(struct aecp *aecp, int64_t now, const void *m,
    int len)
{
    int rc;
	struct server *server = aecp->server;
	const struct avb_ethernet_header *h = m;
	const struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
    struct avb_packet_aecp_aem_setget_name *sg_name;

    /** Information in the packet */
    uint16_t desc_type;
    uint16_t desc_index;
    uint16_t name_index;
    uint64_t ctrler_index;
    char *name;
    char old_name[AECP_AEM_STRLEN_MAX];
    /*Information about the system */
    struct descriptor *desc;

    sg_name = (struct avb_packet_aecp_aem_setget_name*) p->payload;

    desc_type = ntohs(sg_name->descriptor_type);
    desc_index = ntohs(sg_name->descriptor_index);
    name_index = ntohs(sg_name->name_index);
    name = sg_name->name;

    /** Retrieve the descriptor */
	desc = server_find_descriptor(server, desc_type, desc_index);
	if (desc == NULL)
		return reply_status(aecp, AVB_AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR, m, len);

    if (!list_support_descriptors_setget_name[desc_type]) {
        return reply_bad_arguments(aecp, m, len);
    }

    // Store the old name before updating
    memcpy(old_name, (char *)desc->ptr, AECP_AEM_STRLEN_MAX);

    // Handle name setting based on descriptor type
    switch (desc_type) {
        case AVB_AEM_DESC_ENTITY:
            rc = handle_set_name_entity(desc, name_index, name);
            break;
        default:
            rc = handle_set_name_generic(desc, name_index, name);
            break;
    }

    if (rc) {
        pw_log_error("Unexpected failure while setting name for descriptor type %u, index %u", desc->type, desc->index);
        spa_assert(0);
        // TODO: Which status is the correct one for a failure?
        return reply_set_name(aecp, m, len, AVB_AECP_AEM_STATUS_BAD_ARGUMENTS, old_name);
    }

    ctrler_index = htobe64(p->aecp.controller_guid);
    rc = reply_success(aecp, m, len);
    if (rc) {
        pw_log_error("Could not send message\n");
        return -1;
    }
    // Then do unsolicited reply
    return handle_unsol_set_name(aecp, desc, sg_name, ctrler_index);
}