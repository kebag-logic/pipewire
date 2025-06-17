#include "../aecp-aem-state.h"
#include "../descriptors.h"
#include "aecp-aem-helpers.h"
#include "aecp-aem-types.h"
#include "aecp-aem-unsol-helper.h"

#include "aecp-aem-cmd-set-stream-format.h"


static int handle_unsol_set_stream_format(struct aecp *aecp, struct descriptor *desc,
    uint64_t ctrler_id)
{
	/* Reply */
	uint8_t buf[512];
	void *m = buf;
	struct avb_ethernet_header *h = m;
	struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
	struct avb_packet_aecp_aem_setget_stream_format *sg_sf;
    struct avb_aem_desc_stream *desc_stream;
	size_t len = sizeof (*h) + sizeof(*p) + sizeof(*sg_sf);
	int rc;


	// Then make sure that it does not happen again.
    desc_stream = desc->ptr;

    sg_sf = (struct avb_packet_aecp_aem_setget_stream_format*) p->payload;

    sg_sf->descriptor_id = htons(desc->index);
    sg_sf->descriptor_type = htons(desc->type);
    sg_sf->stream_format = desc_stream->current_format;

    /** Setup the packet for the unsolicited notification*/
	AVB_PACKET_AEM_SET_COMMAND_TYPE(p, AVB_AECP_AEM_CMD_SET_STREAM_FORMAT);

    rc = reply_unsolicited_notifications_ctrler_id(aecp, ctrler_id, m, len, false);
    if (rc) {
        spa_assert(0);
    }

    return rc;
}

 /* IEEE 1722.1-2021, Clause 7.4.9. SET_STREAM_FORMAT Command */
int handle_cmd_set_stream_format(struct aecp *aecp, int64_t now, const void *m, int len)
{
    struct server *server = aecp->server;
    const struct avb_ethernet_header *h = m;
    const struct avb_packet_aecp_aem *p = SPA_PTROFF(h, sizeof(*h), void);
    struct avb_packet_aecp_aem_setget_stream_format *sg_sf;
    struct avb_aem_desc_stream *desc_stream;
    struct descriptor *desc;

    uint16_t desc_type;
    uint16_t desc_index;
    uint64_t stream_format;
    uint64_t ctrler_index;
    int rc;

    sg_sf = (struct avb_packet_aecp_aem_setget_stream_format*) p->payload;

    desc_type = ntohs(sg_sf->descriptor_type);
    desc_index = ntohs(sg_sf->descriptor_id);
    stream_format = sg_sf->stream_format;
    ctrler_index = htobe64(p->aecp.controller_guid);

    desc = server_find_descriptor(server, desc_type, desc_index);
	if (desc == NULL)
		return reply_status(aecp, AVB_AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR, m, len);

    desc_stream = desc->ptr;

    for (uint16_t format_idx = 0; format_idx < desc_stream->number_of_formats;
            format_idx++)
    {
        if (desc_stream->stream_formats[format_idx] == stream_format) {
#ifdef USE_MILAN
    // TODO  Milan v1.2 5.4.2.7 Check whether static/dynamic mapping exists
    // And return return reply_bad_arguments(aecp, m, len); wait for mappings
#endif //USE_MILAN
            desc_stream->current_format = stream_format;
            break;
        }
    }

    sg_sf->stream_format = desc_stream->current_format;
    rc = reply_success(aecp, m, len);
    if (rc) {
        pw_log_error("Reply failed while setting stream format\n");
        return -1;
    }

    return handle_unsol_set_stream_format(aecp, desc, ctrler_index);
}