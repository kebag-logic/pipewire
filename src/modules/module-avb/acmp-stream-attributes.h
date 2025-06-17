/* AVB support */
/* SPDX-FileCopyrightText: Copyright © 2025 Kebag-Logic */
/* SPDX-FileCopyrightText: Copyright © 2025 Alex Malki <alexandre.malki@kebag-logic.com> */
/* SPDX-License-Identifier: MIT */

#ifndef __ACMP_ATTRIBUTE_H__
#define __ACMP_ATTRIBUTE_H__

#include <stdint.h>

enum milan_acmp_talker_sta {
    MILAN_AMCP_TALKER_STA_MAX
};

/** Milan v1.2 ACMP */
enum milan_acmp_listener_sta {
    MILAN_ACMP_LISTENER_STA_UNBOUND,
    MILAN_ACMP_LISTENER_STA_PRB_W_AVAIL,
    MILAN_ACMP_LISTENER_STA_PRB_W_DELAY,
    MILAN_ACMP_LISTENER_STA_PRB_W_RESP,
    MILAN_ACMP_LISTENER_STA_PRB_W_RESP2,
    MILAN_ACMP_LISTENER_STA_PRB_W_RETRY,
    MILAN_ACMP_LISTENER_STA_SETTLED_NO_RSV,
    MILAN_ACMP_LISTENER_STA_SETTLED_RSV_OK,

    MILAN_ACMP_LISTENER_STA_MAX,
};

struct fsm_binding_parameters {
    uint32_t status;
    uint64_t controller_entity_id;
    uint64_t talker_entity_id;
    uint64_t listener_entity_id;

    uint16_t talker_unique_id;
    uint16_t listener_unique_id;

    uint16_t sequence_id;

    uint64_t stream_id;
    char stream_dest_mac[6];
    uint8_t stream_vlan_id;
};

struct fsm_state_talker {
    struct spa_list link;

    uint64_t stream_id;
    enum milan_acmp_talker_sta current_state;
    int64_t timeout;
};

struct fsm_state_listener {
    struct spa_list link;

    struct fsm_binding_parameters binding_parameters;

    enum milan_acmp_listener_sta current_state;
    int64_t timeout;
    uint16_t flags;
    uint8_t probing_status;
    uint16_t connection_count;
    uint8_t STREAMING_WAIT;

    // FIXME: Is it necessary? remove if not
    uint8_t buf[2048];
};

#endif // __ACMP_STREAM_ATTRIBUTE_H__