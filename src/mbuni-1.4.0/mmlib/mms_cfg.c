/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMS Config file reader functions
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mms_cfg.h"


/* config file representation.
 * 'grps' var is indexed by group name. 
 * for multi the value is a list of groups,
 * for single it is a struct
 */
struct mCfg {
     Octstr *file;
     Dict *grps;          
};

struct mCfgGrp {
     Octstr *name;
     Dict *fields;
};

static void fixup_value(Octstr *value, int lineno)
{
     Octstr *tmp;
     int  i,n;
     
     octstr_strip_blanks(value);

     if (octstr_get_char(value, 0) != '"')
	  return;
     if (octstr_get_char(value, octstr_len(value) - 1) != '"')
	  error(0, "Missing enclosing '\"' at line %d in conf file", lineno);
     
     octstr_delete(value, 0,1); /* strip quotes. */
     octstr_delete(value, octstr_len(value) - 1, 1);

     tmp = octstr_duplicate(value);     
     octstr_delete(value, 0, octstr_len(value));
     
     for (i = 0, n = octstr_len(tmp); i < n; i++) {
	  int ch = octstr_get_char(tmp, i);

	  if (ch != '\\') {
	       octstr_append_char(value, ch);
	       continue;
	  }

	  i++; /* skip forward. */
	  ch = octstr_get_char(tmp,i);
	  switch(ch) {
	  case '"':
	  case '\\':
	  default:
	       octstr_append_char(value, ch);
	       break;
	  case 'n':
	       octstr_append_char(value, '\n');
	       break;
	  case 't':
	       octstr_append_char(value, '\t');
	       break;
	  
	  }	 
     }     
     octstr_destroy(tmp);
}

static int is_multigroup(Octstr *grpname)
{
#define OCTSTR(x)
#define SINGLE_GROUP(name, fields)					\
     if (octstr_case_compare(grpname, octstr_imm(#name)) == 0) return 0;
#define MULTI_GROUP(name, fields)					\
     if (octstr_case_compare(grpname, octstr_imm(#name)) == 0) return 1;
#include "mms_cfg.def"
     
     return -1;
}

static int valid_in_group(Octstr *grp, Octstr *field)
{     
     /* first validate whether field is permitted in the group. */
#define OCTSTR(parm) else if (octstr_case_compare(field,octstr_imm(#parm)) == 0) return 1;
#define SINGLE_GROUP(grpname, fields) \
     else if (octstr_case_compare(grp, octstr_imm(#grpname)) == 0) { \
	  if (0) (void)0; \
	  fields \
      }
#define MULTI_GROUP(name, fields) SINGLE_GROUP(name, fields)

     if (0)
	  (void)0;
#include "mms_cfg.def"

     return 0;
}

static void check_and_add_field(mCfgGrp *grp, Octstr *field, Octstr *value, int lineno)
{
     if (!valid_in_group(grp->name, field))
	  info(0, "field `%s' is not expected within group `%s' at line %d in conf file - skipped",
	       octstr_get_cstr(field), octstr_get_cstr(grp->name), lineno);
     else if (dict_put_once(grp->fields, field, octstr_duplicate(value)) == 0)
	  panic(0, "Duplicate field `%s' at line %d in conf file", 
		octstr_get_cstr(field), lineno);     
}

mCfg *mms_cfg_read(Octstr *file)
{
     Octstr *sf;
     List *lines;
     int i, n;
     mCfg *cfg;
     mCfgGrp *grp = NULL;
     int skip = 0;
     
     gw_assert(file);

     if ((sf = octstr_read_file(octstr_get_cstr(file))) == NULL) {
	  error(errno, "failed to read config from `%s'", octstr_get_cstr(file));
	  return NULL;
     }

     cfg = gw_malloc(sizeof *cfg);
     cfg->file = octstr_duplicate(file);
     cfg->grps = dict_create(7, NULL);
     
     lines = octstr_split(sf, octstr_imm("\n"));    
     for (i = 0, n = gwlist_len(lines); i < n; i++) {
	  Octstr *current = gwlist_get(lines,i);
	  int pos;
	  
	  octstr_strip_blanks(current);
	  
	  if (octstr_len(current) == 0) { /* end of group. */
	       grp = NULL;
	       skip = 0;
	       continue;
	  } else if (skip || octstr_get_char(current, 0) == '#') 
	       continue; 
	  	  
	  if ((pos = octstr_search_char(current, '=',0)) > 0) {
	       /* a field name. first see if start of grp */
	       Octstr *field = octstr_copy(current,0,pos);
	       Octstr *value = octstr_copy(current,pos+1,octstr_len(current));
	       
	       octstr_strip_blanks(field);
	       fixup_value(value, i+1);
#if 0
	       info(0, "field/value: [%s - %s]", octstr_get_cstr(field), 
		    octstr_get_cstr(value));
#endif

	       if (octstr_case_compare(field, octstr_imm("group")) == 0) 
		    if (grp == NULL) { /* grp name. */		    
			 int ismulti = is_multigroup(value);
			 
			 if (ismulti < 0) {
			      info(0, "Skipping unknown group `%s' at line %d of conf file", 
				   octstr_get_cstr(value), i+1);
			      skip = 1;
			 } else {
			      grp = gw_malloc(sizeof *grp);
			      grp->name = octstr_duplicate(value);
			      grp->fields = dict_create(23, (void (*)(void *))octstr_destroy);
			      
			      if (ismulti) {
				   List *l = dict_get(cfg->grps, value);
				   
				   if (l == NULL) { 
					l = gwlist_create();
					dict_put(cfg->grps, value, l);			      
				   }
				   gwlist_append(l, grp);			 
			      } else if (dict_put_once(cfg->grps, value, grp) == 0)
				   panic(0, "Group `%s' [at line %d] cannot appear more "
					 "than once in config!",
					 octstr_get_cstr(value), i+1);
			 }
		    } else
			 panic(0,"`group' is an illegal field name "
			       "within a group at line %d in config file!",
			       i+1);
	       else  if (grp) /* an ordinary field name. */
		    check_and_add_field(grp, field, value,i+1);
	       else 
		    panic(0, "A group must begin with a `group = group_name' "
			  "clause [at line %d in config file]", i+1);			      	       
	       
	       octstr_destroy(field);
	       octstr_destroy(value);
	  } else
	       panic(0, "mal-formed entry in conf file at line %d!", i+1);
     }

     gwlist_destroy(lines, (gwlist_item_destructor_t *)octstr_destroy);
     octstr_destroy(sf);
     return cfg;
}

static void mGrp_destroy(mCfgGrp *grp)
{
     octstr_destroy(grp->name);    
     dict_destroy(grp->fields);    
     gw_free(grp);
}

void mms_cfg_destroy(mCfg *cfg)
{
     List *l;
     int i, n;

     gw_assert(cfg);
     
     for (i = 0, l  = dict_keys(cfg->grps), n = gwlist_len(l); i < n; i++) {
	  Octstr *grpname = gwlist_get(l, i);
	  void *val = dict_get(cfg->grps, grpname);
	  if (is_multigroup(grpname)) { /* item is a list. */
	       List *gl = val;	  
	int j, m = gwlist_len(gl);
	       for (j = 0; j < m; j++)
		    mGrp_destroy(gwlist_get(gl, j));
	       gwlist_destroy(gl, NULL);
	  } else 
	       mGrp_destroy(val);	  
     }
     gwlist_destroy(l, (gwlist_item_destructor_t *)octstr_destroy);
     dict_destroy(cfg->grps);
     octstr_destroy(cfg->file);
     gw_free(cfg);
}

mCfgGrp *mms_cfg_get_single(mCfg *cfg, Octstr *name)
{
     gw_assert(name);         
     gw_assert(is_multigroup(name) == 0);
     
     return dict_get(cfg->grps, name);
}

List *mms_cfg_get_multi(mCfg *cfg, Octstr *name)
{
     List *l = NULL, *r;
     int i;
     
     gw_assert(name);         
     gw_assert(is_multigroup(name) == 1);
     
     r = dict_get(cfg->grps, name);
     
     if (r)
	  for (i = 0, l = gwlist_create(); i < gwlist_len(r); i++)
	       gwlist_append(l, gwlist_get(r,i));
     return l;
}

Octstr *mms_cfg_get(mCfgGrp *grp, Octstr *name)
{
     Octstr *val;

     gw_assert(grp);

     if (!valid_in_group(grp->name, name))
	  panic(0, "Request for invalid field/variable `%s' in group `%s', unexpected!",
		octstr_get_cstr(name), octstr_get_cstr(grp->name));
     
     val = dict_get(grp->fields, name);
     
     if (!val)
	  return NULL;
     else 
	  return octstr_duplicate(val);
}

int mms_cfg_get_int(mCfgGrp *grp, Octstr *name, long *n)
{
     Octstr *val = mms_cfg_get(grp, name);
     int ret;
     if (!val)
	  return -1;     
     ret = octstr_parse_long(n, val, 0, 0);
     octstr_destroy(val);
     return (ret == -1)  ? -1 : 0;
}


int mms_cfg_get_bool(mCfgGrp *grp, Octstr *name, int *bool)
{
     Octstr *val = mms_cfg_get(grp, name);
     int ret = 0;
     if (!val)
	  return -1;     
     
     if (octstr_case_compare(val, octstr_imm("yes")) == 0 ||
	 octstr_case_compare(val, octstr_imm("true")) == 0 ||
	 octstr_case_compare(val, octstr_imm("1")) == 0)
	  *bool = 1;
     else if (octstr_case_compare(val, octstr_imm("no")) == 0 ||
	 octstr_case_compare(val, octstr_imm("false")) == 0 ||
	 octstr_case_compare(val, octstr_imm("0")) == 0)
	  *bool = 0;
     else {
	  error(0, "Unable to convert value `%s' to boolean for field `%s' in group `%s'",
		octstr_get_cstr(val), octstr_get_cstr(name), octstr_get_cstr(grp->name));
	  ret = -1;
     }
		
     octstr_destroy(val);
     return ret;
}

List *mms_cfg_get_list(mCfgGrp *grp, Octstr *name)
{
     Octstr *val = mms_cfg_get(grp, name);
     List *l;

     if (val == NULL)
	  return NULL;
     l = octstr_split_words(val);
     octstr_destroy(val);
     return l;
}
