/* PipeWire */
/* SPDX-FileCopyrightText: Copyright © 2022 Wim Taymans */
/* SPDX-License-Identifier: MIT */

#ifndef PIPEWIRE_MILAN_H
#define PIPEWIRE_MILAN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pw_context;
struct pw_properties;
struct pw_milan;

struct pw_milan *pw_milan_new(struct pw_context *context,
		struct pw_properties *props, size_t user_data_size);
void pw_milan_destroy(struct pw_milan *milan);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* PIPEWIRE_MILAN_H */
