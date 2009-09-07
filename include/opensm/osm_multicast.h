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
 * 	Declaration of osm_mgrp_t.
 *	This object represents an IBA Multicast Group.
 *	This object is part of the OpenSM family of objects.
 */

#ifndef _OSM_MULTICAST_H_
#define _OSM_MULTICAST_H_

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_fleximap.h>
#include <complib/cl_qlist.h>
#include <complib/cl_spinlock.h>
#include <opensm/osm_base.h>
#include <opensm/osm_mtree.h>
#include <opensm/osm_mcm_port.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/Multicast Group
* NAME
*	Multicast Group
*
* DESCRIPTION
*	The Multicast Group encapsulates the information needed by the
*	OpenSM to manage Multicast Groups.  The OpenSM allocates one
*	Multicast Group object per Multicast Group in the IBA subnet.
*
*	The Multicast Group is not thread safe, thus callers must provide
*	serialization.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/
/****s* OpenSM: Multicast Group/osm_mcast_mgr_ctxt_t
* NAME
*	osm_mcast_mgr_ctxt_t
*
* DESCRIPTION
*	Struct for passing context arguments to the multicast manager.
*
*	The osm_mcast_mgr_ctxt_t object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_mcast_mgr_ctxt {
	cl_list_item_t list_item;
	ib_net16_t mlid;
} osm_mcast_mgr_ctxt_t;
/*
* FIELDS
*
*	mlid
*		The network ordered LID of this Multicast Group
*		(must be >= 0xC000).
*
* SEE ALSO
*********/

/****s* OpenSM: Multicast Group/osm_mgrp_t
* NAME
*	osm_mgrp_t
*
* DESCRIPTION
*	Multicast Group structure.
*
*	The osm_mgrp_t object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_mgrp {
	cl_fmap_item_t map_item;
	ib_net16_t mlid;
	osm_mtree_node_t *p_root;
	cl_qmap_t mcm_port_tbl;
	ib_member_rec_t mcmember_rec;
	boolean_t well_known;
	unsigned full_members;
} osm_mgrp_t;
/*
* FIELDS
*	map_item
*		Map Item for fmap linkage.  Must be first element!!
*
*	mlid
*		The network ordered LID of this Multicast Group (must be
*		>= 0xC000).
*
*	p_root
*		Pointer to the root "tree node" in the single spanning tree
*		for this multicast group.  The nodes of the tree represent
*		switches.  Member ports are not represented in the tree.
*
*	mcm_port_tbl
*		Table (sorted by port GUID) of osm_mcm_port_t objects
*		representing the member ports of this multicast group.
*
*	mcmember_rec
*		Holds the parameters of the Multicast Group.
*
*	well_known
*		Indicates that this is the wellknown multicast group which
*		is created during the initialization of SM/SA and will be
*		present even if there are no ports for this group
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_new
* NAME
*	osm_mgrp_new
*
* DESCRIPTION
*	Allocates and initializes a Multicast Group for use.
*
* SYNOPSIS
*/
osm_mgrp_t *osm_mgrp_new(IN const ib_net16_t mlid);
/*
* PARAMETERS
*	mlid
*		[in] Multicast LID for this multicast group.
*
* RETURN VALUES
*	IB_SUCCESS if initialization was successful.
*
* NOTES
*	Allows calling other Multicast Group methods.
*
* SEE ALSO
*	Multicast Group, osm_mgrp_delete
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_delete
* NAME
*	osm_mgrp_delete
*
* DESCRIPTION
*	Destroys and deallocates a Multicast Group.
*
* SYNOPSIS
*/
void osm_mgrp_delete(IN osm_mgrp_t * const p_mgrp);
/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Multicast Group, osm_mgrp_new
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_is_guid
* NAME
*	osm_mgrp_is_guid
*
* DESCRIPTION
*	Indicates if the specified port GUID is a member of the Multicast Group.
*
* SYNOPSIS
*/
static inline boolean_t
osm_mgrp_is_guid(IN const osm_mgrp_t * const p_mgrp,
		 IN const ib_net64_t port_guid)
{
	return (cl_qmap_get(&p_mgrp->mcm_port_tbl, port_guid) !=
		cl_qmap_end(&p_mgrp->mcm_port_tbl));
}

/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port_guid
*		[in] Port GUID.
*
* RETURN VALUES
*	TRUE if the port GUID is a member of the group,
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_is_empty
* NAME
*	osm_mgrp_is_empty
*
* DESCRIPTION
*	Indicates if the multicast group has any member ports.
*
* SYNOPSIS
*/
static inline boolean_t osm_mgrp_is_empty(IN const osm_mgrp_t * const p_mgrp)
{
	return (cl_qmap_count(&p_mgrp->mcm_port_tbl) == 0);
}

/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
* RETURN VALUES
*	TRUE if there are no ports in the multicast group.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_get_mlid
* NAME
*	osm_mgrp_get_mlid
*
* DESCRIPTION
*	The osm_mgrp_get_mlid function returns the multicast LID of this group.
*
* SYNOPSIS
*/
static inline ib_net16_t osm_mgrp_get_mlid(IN const osm_mgrp_t * const p_mgrp)
{
	return (p_mgrp->mlid);
}

/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
* RETURN VALUES
*	MLID of the Multicast Group.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_add_port
* NAME
*	osm_mgrp_add_port
*
* DESCRIPTION
*	Adds a port to the multicast group.
*
* SYNOPSIS
*/
osm_mcm_port_t *osm_mgrp_add_port(osm_subn_t *subn, osm_log_t *log,
				  IN osm_mgrp_t * const p_mgrp,
				  IN const ib_gid_t * const p_port_gid,
				  IN const uint8_t join_state,
				  IN boolean_t proxy_join);
/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object to initialize.
*
*	p_port_gid
*		[in] Pointer to the GID of the port to add to the multicast group.
*
*	join_state
*		[in] The join state for this port in the group.
*
* RETURN VALUES
*	IB_SUCCESS
*	IB_INSUFFICIENT_MEMORY
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_is_port_present
* NAME
*	osm_mgrp_is_port_present
*
* DESCRIPTION
*	checks a port from the multicast group.
*
* SYNOPSIS
*/

boolean_t
osm_mgrp_is_port_present(IN const osm_mgrp_t * const p_mgrp,
			 IN const ib_net64_t port_guid,
			 OUT osm_mcm_port_t ** const pp_mcm_port);
/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port_guid
*		[in] Port guid of the departing port.
*
*  pp_mcm_port
*     [out] Pointer to a pointer to osm_mcm_port_t
*           Updated to the member on success or NULLed
*
* RETURN VALUES
*	TRUE if port present
*	FALSE if port is not present.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_remove_port
* NAME
*	osm_mgrp_remove_port
*
* DESCRIPTION
*	Removes a port from the multicast group.
*
* SYNOPSIS
*/
void
osm_mgrp_delete_port(IN osm_subn_t * const p_subn,
		     IN osm_log_t * const p_log,
		     IN osm_mgrp_t * const p_mgrp,
		     IN const ib_net64_t port_guid);
/*
* PARAMETERS
*
*  p_subn
*     [in] Pointer to the subnet object
*
*  p_log
*     [in] The log object pointer
*
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port_guid
*		[in] Port guid of the departing port.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

int osm_mgrp_remove_port(osm_subn_t *subn, osm_log_t *log, osm_mgrp_t *mgrp,
			 osm_mcm_port_t *mcm, uint8_t join_state);
void osm_mgrp_cleanup(osm_subn_t *subn, osm_mgrp_t *mpgr);

END_C_DECLS
#endif				/* _OSM_MULTICAST_H_ */
