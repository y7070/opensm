/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of multicast functions.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <opensm/osm_multicast.h>
#include <opensm/osm_mcm_port.h>
#include <opensm/osm_mtree.h>
#include <opensm/osm_inform.h>

/**********************************************************************
 **********************************************************************/
void osm_mgrp_delete(IN osm_mgrp_t * p_mgrp)
{
	osm_mcm_port_t *p_mcm_port;
	osm_mcm_port_t *p_next_mcm_port;

	CL_ASSERT(p_mgrp);

	p_next_mcm_port =
	    (osm_mcm_port_t *) cl_qmap_head(&p_mgrp->mcm_port_tbl);
	while (p_next_mcm_port !=
	       (osm_mcm_port_t *) cl_qmap_end(&p_mgrp->mcm_port_tbl)) {
		p_mcm_port = p_next_mcm_port;
		p_next_mcm_port =
		    (osm_mcm_port_t *) cl_qmap_next(&p_mcm_port->map_item);
		osm_mcm_port_delete(p_mcm_port);
	}
	/* destroy the mtree_node structure */
	osm_mtree_destroy(p_mgrp->p_root);

	free(p_mgrp);
}

/**********************************************************************
 **********************************************************************/
osm_mgrp_t *osm_mgrp_new(IN const ib_net16_t mlid)
{
	osm_mgrp_t *p_mgrp;

	p_mgrp = (osm_mgrp_t *) malloc(sizeof(*p_mgrp));
	if (!p_mgrp)
		return NULL;

	memset(p_mgrp, 0, sizeof(*p_mgrp));
	cl_qmap_init(&p_mgrp->mcm_port_tbl);
	p_mgrp->mlid = mlid;

	return p_mgrp;
}

void osm_mgrp_cleanup(osm_subn_t *subn, osm_mgrp_t *mgrp)
{
	if (mgrp->full_members || mgrp->well_known)
		return;
	subn->mgroups[cl_ntoh16(mgrp->mlid) - IB_LID_MCAST_START_HO] = NULL;
	cl_fmap_remove_item(&subn->mgrp_mgid_tbl, &mgrp->map_item);
	osm_mgrp_delete(mgrp);
}

/**********************************************************************
 **********************************************************************/
static void mgrp_send_notice(osm_subn_t * subn, osm_log_t * log,
			     osm_mgrp_t * mgrp, unsigned num)
{
	ib_mad_notice_attr_t notice;
	ib_api_status_t status;

	notice.generic_type = 0x83;	/* generic SubnMgt type */
	ib_notice_set_prod_type_ho(&notice, 4);	/* A Class Manager generator */
	notice.g_or_v.generic.trap_num = CL_HTON16(num);
	/* The sm_base_lid is saved in network order already. */
	notice.issuer_lid = subn->sm_base_lid;
	/* following o14-12.1.11 and table 120 p726 */
	/* we need to provide the MGID */
	memcpy(&notice.data_details.ntc_64_67.gid,
	       &mgrp->mcmember_rec.mgid, sizeof(ib_gid_t));

	/* According to page 653 - the issuer gid in this case of trap
	   is the SM gid, since the SM is the initiator of this trap. */
	notice.issuer_gid.unicast.prefix = subn->opt.subnet_prefix;
	notice.issuer_gid.unicast.interface_id = subn->sm_port_guid;

	if ((status = osm_report_notice(log, subn, &notice)))
		OSM_LOG(log, OSM_LOG_ERROR, "ERR 7601: "
			"Error sending trap reports (%s)\n",
			ib_get_err_str(status));
}

/**********************************************************************
 **********************************************************************/
osm_mcm_port_t *osm_mgrp_add_port(IN osm_subn_t * subn, osm_log_t * log,
				  IN osm_mgrp_t * p_mgrp,
				  IN const ib_gid_t * p_port_gid,
				  IN const uint8_t join_state,
				  IN boolean_t proxy_join)
{
	ib_net64_t port_guid;
	osm_mcm_port_t *p_mcm_port;
	cl_map_item_t *prev_item;
	uint8_t prev_join_state = 0;
	uint8_t prev_scope;

	p_mcm_port = osm_mcm_port_new(p_port_gid, join_state, proxy_join);
	if (!p_mcm_port)
		return NULL;

	port_guid = p_port_gid->unicast.interface_id;

	/*
	   prev_item = cl_qmap_insert(...)
	   Pointer to the item in the map with the specified key.  If insertion
	   was successful, this is the pointer to the item.  If an item with the
	   specified key already exists in the map, the pointer to that item is
	   returned.
	 */
	prev_item = cl_qmap_insert(&p_mgrp->mcm_port_tbl,
				   port_guid, &p_mcm_port->map_item);

	/* if already exists - revert the insertion and only update join state */
	if (prev_item != &p_mcm_port->map_item) {
		osm_mcm_port_delete(p_mcm_port);
		p_mcm_port = (osm_mcm_port_t *) prev_item;

		/*
		   o15.0.1.11
		   Join state of the end port should be the or of the
		   previous setting with the current one
		 */
		ib_member_get_scope_state(p_mcm_port->scope_state, &prev_scope,
					  &prev_join_state);
		p_mcm_port->scope_state =
		    ib_member_set_scope_state(prev_scope,
					      prev_join_state | join_state);
	}

	if ((join_state & IB_JOIN_STATE_FULL) &&
	    !(prev_join_state & IB_JOIN_STATE_FULL) &&
	    ++p_mgrp->full_members == 1)
		mgrp_send_notice(subn, log, p_mgrp, 66);

	return (p_mcm_port);
}

/**********************************************************************
 **********************************************************************/
int osm_mgrp_remove_port(osm_subn_t * subn, osm_log_t * log, osm_mgrp_t * mgrp,
			 osm_mcm_port_t * mcm, uint8_t join_state)
{
	int ret;
	uint8_t port_join_state;
	uint8_t new_join_state;

	/*
	 * according to the same o15-0.1.14 we get the stored
	 * JoinState and the request JoinState and they must be
	 * opposite to leave - otherwise just update it
	 */
	port_join_state = mcm->scope_state & 0x0F;
	new_join_state = port_join_state & ~join_state;

	if (new_join_state) {
		mcm->scope_state = new_join_state | (mcm->scope_state & 0xf0);
		OSM_LOG(log, OSM_LOG_DEBUG,
			"updating port 0x%" PRIx64 " JoinState 0x%x -> 0x%x\n",
			cl_ntoh64(mcm->port_gid.unicast.interface_id),
			port_join_state, new_join_state);
		ret = 0;
	} else {
		cl_qmap_remove_item(&mgrp->mcm_port_tbl, &mcm->map_item);
		OSM_LOG(log, OSM_LOG_DEBUG, "removing port 0x%" PRIx64 "\n",
			cl_ntoh64(mcm->port_gid.unicast.interface_id));
		osm_mcm_port_delete(mcm);
		ret = 1;
	}

	/* no more full members so the group will be deleted after re-route
	   but only if it is not a well known group */
	if ((port_join_state & IB_JOIN_STATE_FULL) &&
	    !(new_join_state & IB_JOIN_STATE_FULL) &&
	    --mgrp->full_members == 0)
		mgrp_send_notice(subn, log, mgrp, 67);

	return ret;
}

void osm_mgrp_delete_port(osm_subn_t * subn, osm_log_t * log, osm_mgrp_t * mgrp,
			  ib_net64_t port_guid)
{
	cl_map_item_t *item = cl_qmap_get(&mgrp->mcm_port_tbl, port_guid);

	if (item != cl_qmap_end(&mgrp->mcm_port_tbl))
		osm_mgrp_remove_port(subn, log, mgrp, (osm_mcm_port_t *) item,
				     0xf);
	osm_mgrp_cleanup(subn, mgrp);
}

/**********************************************************************
 **********************************************************************/
boolean_t osm_mgrp_is_port_present(IN const osm_mgrp_t * p_mgrp,
				   IN const ib_net64_t port_guid,
				   OUT osm_mcm_port_t ** const pp_mcm_port)
{
	cl_map_item_t *p_map_item;

	CL_ASSERT(p_mgrp);

	p_map_item = cl_qmap_get(&p_mgrp->mcm_port_tbl, port_guid);

	if (p_map_item != cl_qmap_end(&p_mgrp->mcm_port_tbl)) {
		if (pp_mcm_port)
			*pp_mcm_port = (osm_mcm_port_t *) p_map_item;
		return TRUE;
	}
	if (pp_mcm_port)
		*pp_mcm_port = NULL;
	return FALSE;
}
