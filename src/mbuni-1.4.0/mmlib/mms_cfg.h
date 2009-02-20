/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 *  config file functions
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */ 
#ifndef __MMS_CFG__INCLUDED__
#define __MMS_CFG__INCLUDED__

#include "gwlib/gwlib.h"
#include "gwlib/mime.h"

#include "mbuni-config.h"

typedef struct mCfg mCfg; /* config file structure. */
typedef struct mCfgGrp mCfgGrp; /* A config group. */


/* Read a config file, return the structure. */
mCfg *mms_cfg_read(Octstr *file);

/* Destroy it all . */
void mms_cfg_destroy(mCfg *cfg);

/* Get a single group object by name. */
mCfgGrp *mms_cfg_get_single(mCfg *cfg, Octstr  *name);

/* Get a multiple groups all of same  name. */
List *mms_cfg_get_multi(mCfg *cfg,Octstr *name);

/* Get a string field value from a group. */
Octstr *mms_cfg_get(mCfgGrp *grp, Octstr *name);


/* Get an integer field value from a group. 
 * returns 0 on success, -1 on error. 
 */
int mms_cfg_get_int(mCfgGrp *grp, Octstr *name, long *n);

/* Get a boolean field value from a group. 
 * returns 0 on success, -1 on error
 */
int mms_cfg_get_bool(mCfgGrp *grp, Octstr *name, int *bool);

/* Get a field value from a group. */
List *mms_cfg_get_list(mCfgGrp *grp, Octstr *name);


#endif

