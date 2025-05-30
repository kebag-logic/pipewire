/* PipeWire */
/* SPDX-FileCopyrightText: Copyright © 2019 Wim Taymans */
/* SPDX-License-Identifier: MIT */

#include <spa/node/utils.h>
#include <spa/pod/parser.h>
#include <spa/param/param.h>
#include <spa/buffer/alloc.h>
#include <spa/debug/types.h>

#include "pipewire/keys.h"
#include "pipewire/private.h"

#include "buffers.h"

PW_LOG_TOPIC_EXTERN(log_buffers);
#define PW_LOG_TOPIC_DEFAULT log_buffers

#define MAX_ALIGN	32u
#define MAX_BLOCKS	256u

struct port {
	struct spa_node *node;
	enum spa_direction direction;
	uint32_t port_id;
};

/* Allocate an array of buffers that can be shared */
static int alloc_buffers(struct pw_mempool *pool,
			 uint32_t n_buffers,
			 uint32_t n_metas,
			 struct spa_meta *metas,
			 uint32_t n_datas,
			 uint32_t *data_sizes,
			 int32_t *data_strides,
			 uint32_t *data_aligns,
			 uint32_t *data_types,
			 uint32_t flags,
			 struct pw_buffers *allocation)
{
	struct spa_buffer **buffers;
	void *skel, *data;
	uint32_t i;
	struct spa_data *datas;
	struct pw_memblock *m;
	struct spa_buffer_alloc_info info = { 0, };

	if (!SPA_FLAG_IS_SET(flags, PW_BUFFERS_FLAG_SHARED))
		SPA_FLAG_SET(info.flags, SPA_BUFFER_ALLOC_FLAG_INLINE_ALL);

	datas = alloca(sizeof(struct spa_data) * n_datas);

	for (i = 0; i < n_datas; i++) {
		struct spa_data *d = &datas[i];

		spa_zero(*d);
		if (data_sizes[i] > 0) {
			/* we allocate memory */
			d->type = SPA_DATA_MemPtr;
			d->maxsize = data_sizes[i];
			SPA_FLAG_SET(d->flags, SPA_DATA_FLAG_READWRITE);
		} else {
			/* client allocates memory. Set the mask of possible
			 * types in the type field */
			d->type = data_types[i];
			d->maxsize = 0;
		}
		if (SPA_FLAG_IS_SET(flags, PW_BUFFERS_FLAG_DYNAMIC))
			SPA_FLAG_SET(d->flags, SPA_DATA_FLAG_DYNAMIC);
	}

        spa_buffer_alloc_fill_info(&info, n_metas, metas, n_datas, datas, data_aligns);

	buffers = calloc(1, info.max_align + n_buffers * (sizeof(struct spa_buffer *) + info.skel_size));
	if (buffers == NULL)
		return -errno;

	skel = SPA_PTROFF(buffers, n_buffers * sizeof(struct spa_buffer *), void);
	skel = SPA_PTR_ALIGN(skel, info.max_align, void);

	if (SPA_FLAG_IS_SET(flags, PW_BUFFERS_FLAG_SHARED)) {
		/* pointer to buffer structures */
		m = pw_mempool_alloc(pool,
				PW_MEMBLOCK_FLAG_READWRITE |
				PW_MEMBLOCK_FLAG_SEAL |
				PW_MEMBLOCK_FLAG_MAP,
				SPA_DATA_MemFd,
				n_buffers * info.mem_size);
		if (m == NULL) {
			free(buffers);
			return -errno;
		}

		data = m->map->ptr;
	} else {
		m = NULL;
		data = NULL;
	}

	pw_log_debug("%p: layout buffers skel:%p data:%p n_buffers:%d buffers:%p",
			allocation, skel, data, n_buffers, buffers);
	spa_buffer_alloc_layout_array(&info, n_buffers, buffers, skel, data);

	allocation->mem = m;
	allocation->n_buffers = n_buffers;
	allocation->buffers = buffers;
	allocation->flags = flags;

	return 0;
}

static int
param_filter(struct pw_buffers *this,
	     struct port *in_port,
	     struct port *out_port,
	     uint32_t id,
	     struct spa_pod_builder *result)
{
	uint8_t ibuf[4096];
        struct spa_pod_builder ib = { 0 };
	struct spa_pod *oparam, *iparam;
	uint32_t iidx, oidx;
	int in_res = -EIO, out_res = -EIO, num = 0;

	for (iidx = 0;;) {
	        spa_pod_builder_init(&ib, ibuf, sizeof(ibuf));
		pw_log_debug("%p: input param %d id:%d", this, iidx, id);
		in_res = spa_node_port_enum_params_sync(in_port->node,
						in_port->direction, in_port->port_id,
						id, &iidx, NULL, &iparam, &ib);

		if (in_res < 1) {
			/* in_res == -ENOENT  : unknown parameter, assume NULL and we will
			 *                      exit the loop below.
			 * in_res < 1         : some error or no data, exit now
			 */
			if (in_res == -ENOENT)
				iparam = NULL;
			else
				break;
		}

		pw_log_pod(SPA_LOG_LEVEL_DEBUG, iparam);

		for (oidx = 0;;) {
			pw_log_debug("%p: output param %d id:%d", this, oidx, id);
			out_res = spa_node_port_enum_params_sync(out_port->node,
						out_port->direction, out_port->port_id,
						id, &oidx, iparam, &oparam, result);

			/* out_res < 1 : no value or error, exit now */
			if (out_res < 1)
				break;

			pw_log_pod(SPA_LOG_LEVEL_DEBUG, oparam);
			num++;
		}
		if (out_res == -ENOENT && iparam) {
			/* no output param known but we have an input param,
			 * use that one */
			spa_pod_builder_raw_padded(result, iparam, SPA_POD_SIZE(iparam));
			num++;
		}
		/* no more input values, exit */
		if (in_res < 1)
			break;
	}
	if (num == 0) {
		if (out_res == -ENOENT && in_res == -ENOENT)
			return 0;
		if (in_res < 0)
			return in_res;
		if (out_res < 0)
			return out_res;
		return -EINVAL;
	}
	return num;
}

SPA_EXPORT
int pw_buffers_negotiate(struct pw_context *context, uint32_t flags,
		struct spa_node *outnode, uint32_t out_port_id,
		struct spa_node *innode, uint32_t in_port_id,
		struct pw_buffers *result)
{
	struct spa_pod **params;
	uint8_t buffer[4096];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	uint32_t i, j, offset, n_params, n_metas;
	struct spa_meta *metas;
	uint32_t max_buffers, blocks;
	size_t minsize, stride, align;
	uint32_t *data_sizes;
	int32_t *data_strides;
	uint32_t *data_aligns;
	uint32_t types, *data_types;
	struct port output = { outnode, SPA_DIRECTION_OUTPUT, out_port_id };
	struct port input = { innode, SPA_DIRECTION_INPUT, in_port_id };
	int res;

	if (flags & PW_BUFFERS_FLAG_IN_PRIORITY) {
		struct port tmp = output;
		output = input;
		input = tmp;
	}

	/* collect buffers */
	res = param_filter(result, &input, &output, SPA_PARAM_Buffers, &b);
	if (res < 0) {
		pw_context_debug_port_params(context, input.node, input.direction,
				input.port_id, SPA_PARAM_Buffers, res,
				"input param");
		pw_context_debug_port_params(context, output.node, output.direction,
				output.port_id, SPA_PARAM_Buffers, res,
				"output param");
		return res;
	}

	/* collect metadata */
	n_params = res;
	if ((res = param_filter(result, &input, &output, SPA_PARAM_Meta, &b)) > 0)
		n_params += res;

	metas = alloca(sizeof(struct spa_meta) * n_params);

	n_metas = 0;
	params = alloca(n_params * sizeof(struct spa_pod *));
	for (i = 0, offset = 0; i < n_params; i++) {
		uint32_t type, size;

		params[i] = SPA_PTROFF(buffer, offset, struct spa_pod);
		spa_pod_fixate(params[i]);
		pw_log_debug("%p: fixated param %d:", result, i);
		pw_log_pod(SPA_LOG_LEVEL_DEBUG, params[i]);
		offset += SPA_ROUND_UP_N(SPA_POD_SIZE(params[i]), 8);

		if (!spa_pod_is_object_type (params[i], SPA_TYPE_OBJECT_ParamMeta))
			continue;
		if (spa_pod_parse_object(params[i],
					SPA_TYPE_OBJECT_ParamMeta, NULL,
					SPA_PARAM_META_type, SPA_POD_Id(&type),
					SPA_PARAM_META_size, SPA_POD_Int(&size)) < 0) {
			pw_log_warn("%p: invalid Meta param", result);
			continue;
		}

		pw_log_debug("%p: enable meta %s size:%d", result,
				spa_debug_type_find_name(spa_type_meta_type, type), size);

		metas[n_metas].type = type;
		metas[n_metas].size = size;
		n_metas++;
	}

	max_buffers = context->settings.link_max_buffers;

	align = pw_properties_get_uint32(context->properties, PW_KEY_CPU_MAX_ALIGN, MAX_ALIGN);

	minsize = stride = 0;
	types = SPA_ID_INVALID; /* bitmask of allowed types */
	blocks = 1;

	for (i = 0; i < n_params; i++) {
		uint32_t qmax_buffers = max_buffers,
		    qminsize = minsize, qstride = stride, qalign = align;
		uint32_t qtypes = types, qblocks = blocks, qmetas = 0;

		if (!spa_pod_is_object_type (params[i], SPA_TYPE_OBJECT_ParamBuffers))
			continue;

		if (spa_pod_parse_object(params[i],
				SPA_TYPE_OBJECT_ParamBuffers, NULL,
				SPA_PARAM_BUFFERS_buffers,  SPA_POD_OPT_Int(&qmax_buffers),
				SPA_PARAM_BUFFERS_blocks,   SPA_POD_OPT_Int(&qblocks),
				SPA_PARAM_BUFFERS_size,     SPA_POD_OPT_Int(&qminsize),
				SPA_PARAM_BUFFERS_stride,   SPA_POD_OPT_Int(&qstride),
				SPA_PARAM_BUFFERS_align,    SPA_POD_OPT_Int(&qalign),
				SPA_PARAM_BUFFERS_dataType, SPA_POD_OPT_Int(&qtypes),
				SPA_PARAM_BUFFERS_metaType, SPA_POD_OPT_Int(&qmetas)) < 0) {
			pw_log_warn("%p: invalid Buffers param", result);
			continue;
		}
		for (j = 0; qmetas != 0 && j < n_metas; j++)
			SPA_FLAG_CLEAR(qmetas, 1<<metas[j].type);
		if (qmetas != 0)
			continue;

		max_buffers =
		    qmax_buffers == 0 ? max_buffers : SPA_MIN(qmax_buffers,
						      max_buffers);
		blocks = SPA_CLAMP(qblocks, blocks, MAX_BLOCKS);
		minsize = SPA_MAX(minsize, qminsize);
		stride = SPA_MAX(stride, qstride);
		align = SPA_MAX(align, qalign);
		types = qtypes;

		pw_log_debug("%p: %d %d %d %d %d %d -> %d %zd %zd %d %zd %d", result,
				qblocks, qminsize, qstride, qmax_buffers, qalign, qtypes,
				blocks, minsize, stride, max_buffers, align, types);
		break;
	}
	if (i == n_params) {
		pw_log_warn("%p: no buffers param", result);
		minsize = context->settings.clock_quantum_limit;
		max_buffers = 2u;
	}

	if (SPA_FLAG_IS_SET(flags, PW_BUFFERS_FLAG_ASYNC))
		max_buffers = SPA_MAX(2u, max_buffers);

	if (SPA_FLAG_IS_SET(flags, PW_BUFFERS_FLAG_SHARED_MEM)) {
		if (types != SPA_ID_INVALID)
			SPA_FLAG_CLEAR(types, 1<<SPA_DATA_MemPtr);
		if (types == 0 || types == SPA_ID_INVALID)
			types = 1<<SPA_DATA_MemFd;
	}

	if (SPA_FLAG_IS_SET(flags, PW_BUFFERS_FLAG_NO_MEM))
		minsize = 0;

	data_sizes = alloca(sizeof(uint32_t) * blocks);
	data_strides = alloca(sizeof(int32_t) * blocks);
	data_aligns = alloca(sizeof(uint32_t) * blocks);
	data_types = alloca(sizeof(uint32_t) * blocks);

	for (i = 0; i < blocks; i++) {
		data_sizes[i] = minsize;
		data_strides[i] = stride;
		data_aligns[i] = align;
		data_types[i] = types;
	}

	if ((res = alloc_buffers(context->pool,
				 max_buffers,
				 n_metas,
				 metas,
				 blocks,
				 data_sizes, data_strides,
				 data_aligns, data_types,
				 flags,
				 result)) < 0) {
		pw_log_error("%p: can't alloc buffers: %s", result, spa_strerror(res));
	}

	return res;
}

SPA_EXPORT
void pw_buffers_clear(struct pw_buffers *buffers)
{
	pw_log_debug("%p: clear %d buffers:%p", buffers, buffers->n_buffers, buffers->buffers);
	if (buffers->mem)
		pw_memblock_unref(buffers->mem);
	free(buffers->buffers);
	spa_zero(*buffers);
}
