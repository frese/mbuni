/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMSC handler functions: Receive and send MMS messages to MMSCs 
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */
#include <sys/file.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <dlfcn.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mmsbox.h"
#include "mms_queue.h"

#include "mmsbox.h"
#include "mms_msg.h"

#define MOD_SUBJECT(msg, mmc,xfrom) do { \
      if ((mmc)->reroute_mod_subject) { \
          Octstr *s = mms_get_header_value((msg),octstr_imm("Subject")); \
          Octstr *f = octstr_duplicate(xfrom); \
          int _i; \
          if (s == NULL) s = octstr_create(""); \
          if ((_i = octstr_search(f, octstr_imm("/TYPE="), 0)) >= 0) \
              octstr_delete(f, _i, octstr_len(f)); \
          octstr_format_append(s, " (from %S)", (f)); \
          mms_replace_header_value((msg), "Subject", octstr_get_cstr(s)); \
          octstr_destroy(s); octstr_destroy(f); \
       } \
} while(0)

typedef struct MmsHTTPClientInfo {
     HTTPClient *client;
     Octstr *ua;
     Octstr *ip;
     List   *headers;
     Octstr *url;
     Octstr *body;
     List   *cgivars;
     MmscGrp *m;     
} MmsHTTPClientInfo;

typedef struct MmsMsgRef {
	time_t timestamp;
	Octstr *msgid;
	Octstr *url;
} MmsMsgRef;

static void free_clientInfo(MmsHTTPClientInfo *h, int freeh);

extern List* substitution_patterns;

extern long eaif_http_accept_response;

static List* eaif_pending_async_response = NULL; // Created by response thread

static void msgref_dump(MmsMsgRef *ref) {
	debug("", 0, "Dumping mms msg ref at #%ld", (long)ref);
	octstr_dump(ref->msgid, 0);
	octstr_dump(ref->url, 0);
	debug("", 0, "Timestamp: %ld", ref->timestamp);
}

static MmsMsgRef *msgref_create(Octstr *msgid, Octstr *url) {
	MmsMsgRef* ref = gw_malloc(sizeof(struct MmsMsgRef));
	//debug("", 0, "Allocating new msgref with addr %ld (msgid=%s, url=%s)", (long)ref, msgid ? octstr_get_cstr(msgid) : "null", url ? octstr_get_cstr(url) : "null");
	ref->msgid = octstr_duplicate(msgid);
	ref->url = octstr_duplicate(url);
	ref->timestamp = time(NULL);
	
	msgref_dump(ref);
	
	return ref;
}

static void msgref_destroy(MmsMsgRef *msgref) {
	gw_assert(msgref);
	//debug("", 0, "Destroying msgref with addr %ld", (long)msgref);
	octstr_destroy(msgref->msgid);
	octstr_destroy(msgref->url);
	gw_free(msgref);
}



void apply_number_substitutions(Octstr **num, List* s_patterns) {
	int i,j;
	Octstr *cur_pattern_id, *tmp;
	SubPattern *cur_pattern = NULL;

	for (i = 0; i < gwlist_len(s_patterns); i++) {
		cur_pattern_id = gwlist_get(s_patterns, i);

		// find the pattern
		for (j = 0; j < gwlist_len(substitution_patterns); j++) {
			cur_pattern = gwlist_get(substitution_patterns,i);
			if (octstr_compare(cur_pattern->id, cur_pattern_id)==0)
				break;
		}
		
		// This should never happen, since it is also checked by configuration parser
		if (cur_pattern == NULL)
			panic(0,"in apply_number_substitutions: No pattern matching %s found!",
				octstr_get_cstr(cur_pattern_id));
		
		//debug("",0,"testing substitution pattern: %s", octstr_get_cstr(cur_pattern->id));
		
		// Check that it matches
		if (!gw_regex_match_pre(cur_pattern->match_pattern, *num))
			continue;
		
		//debug("",0, "applying substitution pattern: %s", octstr_get_cstr(cur_pattern->id));
		
		// Apply the substitution
		if ((tmp = gw_regex_subst_pre(cur_pattern->match_pattern, *num, cur_pattern->replacement))) {
			debug("",0,"substitution [%s] ==> [%s]", octstr_get_cstr(*num), octstr_get_cstr(tmp));
			octstr_destroy(*num);
			*num = tmp;
		} else {
			error(0, "Could not apply number substitution pattern: %s", 
				octstr_get_cstr(cur_pattern->id));
		}
	}
}

static int auth_check(Octstr *user, Octstr *pass, List *headers)
{
     int i, res = -1;
     Octstr *v = http_header_value(headers, octstr_imm("Authorization"));
     Octstr *p = NULL, *q = NULL;

     if (user == NULL ||
	 octstr_len(user) == 0) {
	  res = 0;
	  goto done;
     }

     if (!v ||
	 octstr_search(v, octstr_imm("Basic "), 0) != 0)
	  goto done;
     p = octstr_copy(v, sizeof "Basic", octstr_len(v));
     octstr_base64_to_binary(p);
     
     i = octstr_search_char(p, ':', 0);
     q = octstr_copy(p, i+1, octstr_len(p));
     octstr_delete(p, i, octstr_len(p));
     
     /* p = user, q = pass. */

     if (octstr_compare(user, p) != 0 ||
	 octstr_compare(pass, q) != 0)
	  res = -1;
     else 
	  res = 0;
done:
     octstr_destroy(v);
     octstr_destroy(p);     
     octstr_destroy(q);
     return res;
}

int mmsbox_send_report(Octstr *from, char *report_type,
		       Octstr *dlr_url, Octstr *status, 
		       Octstr *msgid, Octstr *mmc_id, Octstr *mmc_gid, 
		       Octstr *orig_transid, Octstr *uaprof, 
		       time_t uaprof_tstamp)
{
     Octstr *url = NULL;
     List *rh, *rph = NULL;
     Octstr *rb = NULL;
     Octstr *xtransid = NULL;

     if (dlr_url)
	  url = octstr_duplicate(dlr_url);
     else 
	  mms_dlr_url_get(msgid, report_type, mmc_gid, &url, &xtransid);
     
     if (!url || octstr_len(url) == 0) {
	  info(0, "Sending delivery-report skipped: `url' is empty, `group_id'=[%s], `msgid'=[%s], report-type=[%s], status=[%s]",
	       octstr_get_cstr(mmc_gid), octstr_get_cstr(msgid),report_type,octstr_get_cstr(status));
	  return 0;
     }

     rh = http_create_empty_headers();

     http_header_add(rh, "X-Mbuni-Report-Type", report_type);
     http_header_add(rh, "X-Mbuni-MM-Status", octstr_get_cstr(status));
     http_header_add(rh, "X-Mbuni-MMSC-ID", octstr_get_cstr(mmc_id));
     http_header_add(rh, "X-Mbuni-MMSC-GID", octstr_get_cstr(mmc_gid)); 
     http_header_add(rh, "X-Mbuni-From", octstr_get_cstr(from));

     if (xtransid || orig_transid)
	  http_header_add(rh, "X-Mbuni-TransactionID", 
			  octstr_get_cstr(xtransid ? xtransid : orig_transid));
     if (msgid)
	  http_header_add(rh, "X-Mbuni-Message-ID", octstr_get_cstr(msgid));
     if (uaprof) {
	  Octstr *sx = date_format_http(uaprof_tstamp);
	  http_header_add(rh, "X-Mbuni-UAProf", octstr_get_cstr(uaprof));
	  http_header_add(rh, "X-Mbuni-Timestamp", octstr_get_cstr(sx));
	  octstr_destroy(sx);
     }
     mms_url_fetch_content(HTTP_METHOD_GET, url, rh, octstr_imm(""), &rph, &rb);
     
     octstr_destroy(rb);	  
     octstr_destroy(url);
     octstr_destroy(xtransid);

     http_destroy_headers(rph);
     http_destroy_headers(rh);

     /* At what point do we delete it? For now, when we get a read report, 
      * and also when we get  a delivery report that is not 'deferred' or sent
      */
     if (strcmp(report_type, "read-report") == 0 ||
	 (octstr_case_compare(status, octstr_imm("Deferred")) != 0 &&
	  octstr_case_compare(status, octstr_imm("Sent")) != 0))
	  mms_dlr_url_remove(msgid, report_type, mmc_gid);
     return 0;
}

/* These functions are very similar to those in mmsproxy */
static void mm7soap_receive(MmsHTTPClientInfo *h)
{

     MSoapMsg_t *mreq = NULL, *mresp = NULL;
     int hstatus = HTTP_OK;
     List *rh = NULL;
     Octstr *reply_body = NULL;
     
     List *to = NULL;
     Octstr *from = NULL, *subject = NULL,  *vasid = NULL, *msgid = NULL, *uaprof = NULL;
     time_t expiryt = -1, delivert = -1, uaprof_tstamp = -1;
     MmsMsg *m = NULL;
     int status = 1000;
     unsigned char *msgtype = (unsigned char *)"";
     Octstr *qf = NULL, *mmc_id = NULL, *qdir = NULL;
     
     if (h->body)
	  mreq = mm7_parse_soap(h->headers, h->body);

     if (mreq)
	  msgtype = mms_mm7tag_to_cstr(mm7_msgtype(mreq));
     debug("mmsbox.mm7sendinterface", 0,
	   " --> Enterred mm7dispatch interface, mreq=[%s] mtype=[%s] <-- ",
	   mreq ? "Ok" : "Null",
	   mreq ? (char *)msgtype : "Null");
          
     if (!mreq) {
	  mresp = mm7_make_resp(NULL, MM7_SOAP_FORMAT_CORRUPT, NULL,1);
	  goto done;
     }

     mm7_get_envelope(mreq, &from, &to, &subject, &vasid, 
		      &expiryt, &delivert, &uaprof, &uaprof_tstamp);
     
     if (!from)
	  from = octstr_imm("anon@anon");
	
	// Apply regex substitutions on the numbers
	apply_number_substitutions(&from,h->m->incoming_substitution_patterns);
	{ // Also, apply to every entry in to list
		int i;
		List *new_to = gwlist_create();
		for (i = 0; i < gwlist_len(to); i++) {
			Octstr *one_to = octstr_duplicate(gwlist_get(to,i));
			apply_number_substitutions(&one_to,h->m->incoming_substitution_patterns);
			gwlist_append(new_to, one_to);
		}
		gwlist_destroy(to, (gwlist_item_destructor_t *)octstr_destroy);
		to = new_to;

		debug("",0, "Reporting on fields after substitutions:");
		debug("",0, "FROM: %s", octstr_get_cstr(from));

		for (i = 0; i < gwlist_len(to); i++) {
			Octstr *one_to = gwlist_get(to,i);
			debug("",0,"TO[%d]: %s", i, octstr_get_cstr(one_to));
		}
	}
		
     qdir = get_mmsbox_queue_dir(from, to, h->m, &mmc_id); /* get routing info. */

     switch (mm7_msgtype(mreq)) {
     case MM7_TAG_DeliverReq:
	  m = mm7_soap_to_mmsmsg(mreq, from); 
	  if (m) {
	       /* Store linked id so we use it in response. */
	       Octstr *linkedid = mm7_soap_header_value(mreq, octstr_imm("LinkedID"));
	       List *qh = http_create_empty_headers();
	       int dlr;
	       Octstr *value = mms_get_header_value(m, octstr_imm("X-Mms-Delivery-Report"));	  

	       if (value && 
		   octstr_case_compare(value, octstr_imm("Yes")) == 0) 
		    dlr = 1;
	       else 
		    dlr = 0;
	       
	       if (delivert < 0)
		    delivert = time(NULL);
	       
	       if (expiryt < 0)
		    expiryt = time(NULL) + DEFAULT_EXPIRE;
	       
	       if (uaprof) {
		    Octstr *sx = date_format_http(uaprof_tstamp);
		    http_header_add(qh, "X-Mbuni-UAProf", octstr_get_cstr(uaprof));
		    http_header_add(qh, "X-Mbuni-Timestamp", octstr_get_cstr(sx));
		    octstr_destroy(sx);
	       }
	       
	       MOD_SUBJECT(m, h->m, from);
	       
	       qf = qfs->mms_queue_add(from, to, subject,
				       h->m->id, mmc_id,
				       delivert, expiryt, m, linkedid,
				       NULL, NULL,
				       NULL, NULL,
				       qh,
				       dlr, 
				       octstr_get_cstr(qdir),
				       "MM7/SOAP-IN",
				       octstr_imm(MM_NAME));
	       msgid = mms_make_msgid(octstr_get_cstr(qf), octstr_imm(MM_NAME));
	       mms_log("Received", from, to, -1, msgid, NULL, h->m->id, "MMSBox", h->ua, NULL);

	       octstr_destroy(linkedid);
	       octstr_destroy(value);	      
	       http_destroy_headers(qh);
	  }  else {
	       error(0,
		     "Failed to convert received MM7/SOAP DeliverReq message from mmc=%s to MMS Message!",
		       octstr_get_cstr(h->m->id));
	       status = 4000;	  
	  }
	  mresp = mm7_make_resp(mreq, status, NULL,1);

	  break; 	  
	  
     case MM7_TAG_DeliveryReportReq:
	  if (mmc_id != NULL) { /* internal routing. */
	       m = mm7_soap_to_mmsmsg(mreq, from); 
	       if (m)
		    qf = qfs->mms_queue_add(from, to, NULL, 
					    h->m->id, mmc_id,
					    0, time(NULL) + default_msgexpiry, m, NULL, 
					    NULL, NULL,
					    NULL, NULL,
					    NULL,
					    0,
					    octstr_get_cstr(qdir), 				  
					    "MM7/SOAP-IN",
					    octstr_imm(MM_NAME));
	       else 
		    qf = NULL;
	       if (qf)  
		    /* Log to access log */
		    mms_log("Received DLR", from, to, -1, NULL, NULL, h->m->id, "MMSBox", h->ua, NULL);
	       else 
		    status = 4000;
	  } else {
	       Octstr *desc = mm7_soap_header_value(mreq, octstr_imm("StatusText"));
	       Octstr *value = mm7_soap_header_value(mreq, octstr_imm("MMStatus"));

	       msgid = mm7_soap_header_value(mreq, octstr_imm("MessageID"));
	       
	       info(0, "Sending delivery-report [FROM:%s] [VALUE:%s] [DESC:%s] [MSGID:%s]",
		    octstr_get_cstr(from), octstr_get_cstr(value), octstr_get_cstr(desc),
		    octstr_get_cstr(h->m->id));	  
	       mmsbox_send_report(from, "delivery-report", NULL,
			   value, msgid, h->m->id, h->m->group_id, NULL, uaprof, uaprof_tstamp);
	       
	       mms_log("DeliveryReport", 
		       from, NULL, -1, msgid, NULL, h->m->id, "MMSBox", h->ua, NULL);	  
	       octstr_destroy(desc);	  
	       octstr_destroy(value);
	  }
	  mresp = mm7_make_resp(mreq, status, NULL,1);
	  break;
     
     case MM7_TAG_ReadReplyReq:
	  if (mmc_id != NULL) { /* internal routing. */
	       m = mm7_soap_to_mmsmsg(mreq, from); 
	       if (m) {
		
		    qf = qfs->mms_queue_add(from, to, NULL, 
					    h->m->id, mmc_id,
					    0, time(NULL) + default_msgexpiry, m, NULL, 
					    NULL, NULL,
					    NULL, NULL,
					    NULL,
					    0,
					    octstr_get_cstr(qdir), 				  
					    "MM7/SOAP-IN",
					    octstr_imm(MM_NAME));
	       } else 
		    qf = NULL;
	       if (qf)  
		    /* Log to access log */
		    mms_log("Received RR", from, to, -1, NULL, NULL, h->m->id, "MMSBox", h->ua, NULL);		    
	       else 
		    status = 4000;
	  } else {	       
	       Octstr *value = mm7_soap_header_value(mreq, octstr_imm("MMStatus"));
	       msgid = mm7_soap_header_value(mreq, octstr_imm("MessageID"));
	       
	       mmsbox_send_report(from, 
			   "read-report", NULL, value, msgid, 
			   h->m->id, h->m->group_id, NULL, uaprof, uaprof_tstamp);
	       
	       octstr_destroy(value);
	       mms_log("ReadReport", 
		       from, NULL, -1, msgid, NULL, h->m->id, "MMSBox", h->ua, NULL);
	  }     
	  mresp = mm7_make_resp(mreq, status, NULL,1);
	  break;
	  
     default:
	  mresp = mm7_make_resp(mreq, MM7_SOAP_UNSUPPORTED_OPERATION, NULL,1);
	  break;	  
     }
     
 done:
     if (mresp && mm7_soapmsg_to_httpmsg(mresp, &h->m->ver, &rh, &reply_body) == 0) 
	  http_send_reply(h->client, hstatus, rh, reply_body);
     else 
	  http_close_client(h->client);

     debug("mmsbox.mm7sendinterface", 0,
	   " --> leaving mm7dispatch interface, mresp=[%s], body=[%s], mm7_status=[%d] <-- ",
	   mresp ? "ok" : "(null)",
	   reply_body ? "ok" : "(null)", status);
     
	 debug("",0, "octstr_destroy(from);");
     octstr_destroy(from);
	 debug("",0, "octstr_destroy(subject);");
     octstr_destroy(subject);
     octstr_destroy(vasid);
     octstr_destroy(msgid);
     octstr_destroy(qf);
     octstr_destroy(uaprof);
     mms_destroy(m);
     http_destroy_headers(rh);
     octstr_destroy(reply_body);
     mm7_soap_destroy(mresp);
     mm7_soap_destroy(mreq);
	 debug("", 0, "gwlist_destroy(to, ...);");
     gwlist_destroy(to, (gwlist_item_destructor_t *)octstr_destroy);
     octstr_destroy(mmc_id);
}



static void mm7eaif_receive(MmsHTTPClientInfo *h)
{
     MmsMsg *m = NULL;
     List *mh = NULL;
     int hstatus = eaif_http_accept_response;
     List *rh = http_create_empty_headers();
     Octstr *reply_body = NULL, *value;
     Octstr *async_reply_url;
     MmsMsgRef* msgref = NULL;

	 if (eaif_http_accept_response != -1) {
     	if (h->m->asynchronous)
			hstatus = 202;
		else 
			hstatus = 204;
     } else
		hstatus = eaif_http_accept_response;
     
     List *to = gwlist_create(), *hto = NULL;
     Octstr *subject = NULL,  *otransid = NULL, *msgid = NULL;
     Octstr *hfrom = NULL;
     time_t expiryt = -1, deliveryt = -1;
     Octstr *qf = NULL, *xver, *mmc_id = NULL, *qdir = NULL;
     int msize = h->body ? octstr_len(h->body) : 0;
     int dlr;
     int mtype;
     
     debug("mmsbox.mm7eaif.sendinterface", 0, " --> Enterred eaif send interface, blen=[%d] <--- ", msize);

     hfrom = http_header_value(h->headers, octstr_imm("X-NOKIA-MMSC-From"));
	 async_reply_url = http_header_value(h->headers, octstr_imm("X-NOKIA-MMSC-Asynch-Reply-To"));
	
     if (!h->body ||  /* A body is required, and must parse */
	 (m = mms_frombinary(h->body, hfrom ? hfrom : octstr_imm("anon@anon"))) == NULL) {
	  http_header_add(rh, "Content-Type", "text/plain"); 
	  hstatus = HTTP_BAD_REQUEST;
	  reply_body = octstr_format("Unexpected MMS message, no body?");
	  
	  goto done;
     }



     /* XXXX handle delivery reports differently. */
     mtype = mms_messagetype(m);
     mh = mms_message_headers(m);
     /* Now get sender and receiver data. 
      * for now we ignore adaptation flags. 
      */
     mms_collect_envdata_from_msgheaders(mh, &to, &subject, 
					 &otransid, &expiryt, &deliveryt, 
					 DEFAULT_EXPIRE,
					 octstr_get_cstr(unified_prefix), 
					 strip_prefixes);
     
     if ((hto = http_header_find_all(h->headers, "X-NOKIA-MMSC-To")) != NULL && gwlist_len(hto) > 0) { /* To address is in headers. */
	  int i, n;
	  
	  gwlist_destroy(to, (gwlist_item_destructor_t *)octstr_destroy);
	  to = gwlist_create();
	  for (i = 0, n = gwlist_len(hto); i < n; i++) {
	       Octstr *h = NULL, *v = NULL;
	       List *l;
	       int j, m;
	       
	       http_header_get(hto,i,  &h, &v);	       
	       l = http_header_split_value(v);
	       
	       for (j = 0, m = gwlist_len(l); j < m; j++)
		    gwlist_append(to, gwlist_get(l, j));
	       
	       gwlist_destroy(l, NULL);
	       octstr_destroy(h);	       
	       octstr_destroy(v);	       	       
	  }
	  
     }
	
	// Apply regex substitutions on the numbers
	apply_number_substitutions(&hfrom,h->m->incoming_substitution_patterns);
	{ // Also, apply to every entry in to list
		int i;
		List *new_to = gwlist_create();
		for (i = 0; i < gwlist_len(to); i++) {
			Octstr *one_to = octstr_duplicate(gwlist_get(to,i));
			apply_number_substitutions(&one_to,h->m->incoming_substitution_patterns);
			gwlist_append(new_to, one_to);
		}
		gwlist_destroy(to, (gwlist_item_destructor_t *)octstr_destroy);
		to = new_to;

		debug("",0, "Reporting on fields after substitutions:");
		debug("",0, "FROM: %s", octstr_get_cstr(hfrom));

		for (i = 0; i < gwlist_len(to); i++) {
			Octstr *one_to = gwlist_get(to,i);
			debug("",0,"TO[%d]: %s", i, octstr_get_cstr(one_to));
		}
	}
	
     
     qdir = get_mmsbox_queue_dir(hfrom, to, h->m, &mmc_id); /* get routing info. */
     
     switch(mtype) {
     case MMS_MSGTYPE_SEND_REQ:
     case MMS_MSGTYPE_RETRIEVE_CONF:
       
	  /* Get Message ID */
	  if ((msgid = http_header_value(h->headers, octstr_imm("X-NOKIA-MMSC-Message-Id"))) == NULL)
	       msgid = http_header_value(mh, octstr_imm("Message-ID"));
	  else 
	       mms_replace_header_value(m, "Message-ID", octstr_get_cstr(msgid)); /* replace it in the message.*/
	
	  info(0, "Received message has msgid <%s>", octstr_get_cstr(msgid));
	
	  value = http_header_value(mh, octstr_imm("X-Mms-Delivery-Report"));
	  if (value && 
	      octstr_case_compare(value, octstr_imm("Yes")) == 0) 
	       dlr = 1;
	  else 
	       dlr = 0;
	  octstr_destroy(value);
	  
	  if (deliveryt < 0)
	       deliveryt = time(NULL);
	  
	  if (expiryt < 0)
	       expiryt = time(NULL) + DEFAULT_EXPIRE;
	  
	  if (hfrom == NULL)
	       hfrom = http_header_value(mh, octstr_imm("From"));
	  
	  mms_remove_headers(m, "Bcc");
	  mms_remove_headers(m, "X-Mms-Delivery-Time");
	  mms_remove_headers(m, "X-Mms-Expiry");
	  mms_remove_headers(m, "X-Mms-Sender-Visibility");
	  
	  MOD_SUBJECT(m, h->m, hfrom);
	
	if (h->m->seen_messages && NULL != dict_get(h->m->seen_messages, msgid)) {
		warning(0, "Detected duplicate of message with id <%s>", octstr_get_cstr(msgid));
		goto done;
	} 

		
	  /* Save it,  put message id in header, return. */     
	  qf = qfs->mms_queue_add(hfrom, to, subject, 
				  h->m->id, mmc_id,
				  deliveryt, expiryt, m, NULL, 
				  NULL, NULL,
				  NULL, NULL,
				  NULL,
				  dlr,
				  octstr_get_cstr(qdir),
				  "MM7/EAIF-IN",
				  octstr_imm(MM_NAME));
	  
	  if (qf) {
	       /* Log to access log */
		mms_log("Received", hfrom, to, msize, msgid, NULL, h->m->id, "MMSBox", h->ua, NULL);
		
		if (h->m->seen_messages) {
			debug("", 0, "Adding msgid <%s> to msg duplication list.", octstr_get_cstr(msgid));
			msgref = msgref_create(msgid, async_reply_url ? async_reply_url : h->m->mmsc_url);
			dict_put(h->m->seen_messages, msgid, msgref);
		}
	  } else
	       hstatus = HTTP_INTERNAL_SERVER_ERROR;
	  break;
     case MMS_MSGTYPE_DELIVERY_IND:
	  if (mmc_id != NULL) { /* internal routing. */
	       qf = qfs->mms_queue_add(hfrom, to, NULL, 
				       h->m->id, mmc_id,
				       0, time(NULL) + default_msgexpiry, m, NULL, 
				       NULL, NULL,
				       NULL, NULL,
				       NULL,
				       0,
				       octstr_get_cstr(qdir), 				  
				       "MM7/EAIF-IN",
				       octstr_imm(MM_NAME));
	       if (qf)  {
		    /* Log to access log */
		    mms_log("Received DLR", hfrom, to, -1, NULL, NULL, h->m->id, "MMSBox", h->ua, NULL);
	       }  else 
		    hstatus = HTTP_INTERNAL_SERVER_ERROR;
	  } else {
	       Octstr *value = http_header_value(mh, octstr_imm("X-Mms-Status"));
	       Octstr *value2 = http_header_value(mh, octstr_imm("Message-ID")); 
	       mmsbox_send_report(hfrom, "delivery-report", NULL, value, value2, h->m->id, h->m->group_id, NULL, NULL, -1);
	       
	       octstr_destroy(value);
	       octstr_destroy(value2);
	  }
	  break;
	  
     case MMS_MSGTYPE_READ_ORIG_IND:

	  if (mmc_id != NULL) { /* internal routing. */		
	       qf = qfs->mms_queue_add(hfrom, to, NULL, 
				       h->m->id, mmc_id,
				       0, time(NULL) + default_msgexpiry, m, NULL, 
				       NULL, NULL,
				       NULL, NULL,
				       NULL,
				       0,
				       octstr_get_cstr(qdir), 				  
				       "MM7/EAIF-IN",
				       octstr_imm(MM_NAME));
	       if (qf)  {
		   	/* Log to access log */
			mms_log("Received RR", hfrom, to, -1, NULL, NULL, h->m->id, "MMSBox", h->ua, NULL);
	       }  else
		   	hstatus = HTTP_INTERNAL_SERVER_ERROR;
	  } else {
	       Octstr *value = http_header_value(mh, octstr_imm("X-Mms-Read-Status"));
	       Octstr *value2 = http_header_value(mh, octstr_imm("Message-ID")); 
	       mmsbox_send_report(hfrom, "read-report", NULL, value, value2, h->m->id,
			   h->m->group_id, NULL, NULL, -1);
	       
	       
	       octstr_destroy(value);
	       octstr_destroy(value2);
	  }
	  break;
     }

 done:
     
     xver = octstr_format(EAIF_VERSION, h->m->ver.major, h->m->ver.minor1);
     http_header_add(rh, "X-NOKIA-MMSC-Version", octstr_get_cstr(xver));
     octstr_destroy(xver);

     http_send_reply(h->client, hstatus, rh, octstr_imm(""));
     http_destroy_headers(hto);

	 if (msgref && h->m->asynchronous) {
		debug("", 0, "Producing eaif response:");
		msgref_dump(msgref);
		gwlist_produce(eaif_pending_async_response, msgref);	
	 }

     gwlist_destroy(to, (gwlist_item_destructor_t *)octstr_destroy);
     octstr_destroy(hfrom);     
     octstr_destroy(subject);
     octstr_destroy(otransid);
     octstr_destroy(msgid);
     octstr_destroy(qf);
     octstr_destroy(mmc_id);

     http_destroy_headers(mh);
     mms_destroy(m);              
}



void mmsc_receive_func(MmscGrp *m)
{

     MmsHTTPClientInfo h = {NULL};
     Octstr *err = NULL;
     
     h.m = m;
     while(rstop == 0 &&
	   (h.client = http_accept_request(m->incoming.port, 
					   &h.ip, &h.url, &h.headers, 
					   &h.body, &h.cgivars)) != NULL) 
	  if (is_allowed_ip(m->incoming.allow_ip, m->incoming.deny_ip, h.ip)) {

	       h.ua = http_header_value(h.headers, octstr_imm("User-Agent"));	       
	       debug("mmsbox", 0, 
		     " MM7 Incoming, IP=[%s], MMSC=[%s], dest_port=[%ld] ", 
		     h.ip ? octstr_get_cstr(h.ip) : "", 
		     octstr_get_cstr(m->id),
		     m->incoming.port);  
	       
	       /* Dump headers, url etc. */
#if 0
	       http_header_dump(h.headers);
	       if (h.body) octstr_dump(h.body, 0);
	       if (h.ip) octstr_dump(h.ip, 0);
#endif
	       if (auth_check(m->incoming.user,
			      m->incoming.pass, 
			      h.headers) != 0) { /* Ask it to authenticate... */
		    List *hh = http_create_empty_headers();
		    http_header_add(hh, "$enticate", 
				    "Basic realm=\"" MM_NAME "\"");
		    http_send_reply(h.client, HTTP_UNAUTHORIZED, hh, 
				    octstr_imm("Authentication failed"));   
		    http_destroy_headers(hh);
		    info(0, "MMSBox: Auth failed, incoming connection, MMC group=[%s]",
			 m->id ? octstr_get_cstr(m->id) : "(none)");
	       } else if (m->type == SOAP_MMSC || m->type == MPES_MMSC)
		   	mm7soap_receive(&h);
	       else 
		    mm7eaif_receive(&h);
			counter_increase(mms_recieved_counter);
	       free_clientInfo(&h, 0);
	  } else {
	       h.ua = http_header_value(h.headers, octstr_imm("User-Agent"));
	       err = octstr_format("HTTP: Incoming IP denied MMSC[%s] ip=[%s], ua=[%s], disconnected",
				   m->id ? octstr_get_cstr(m->id) : "(none)", 
				   h.ip ? octstr_get_cstr(h.ip) : "(none)",
				   h.ua ? octstr_get_cstr(h.ua) : "(none)");
               if (err) {
                    error(0, "%s", octstr_get_cstr(err));
                    octstr_destroy(err);
               }
               http_send_reply(h.client, HTTP_FORBIDDEN, NULL,
                               octstr_imm("Access denied."));

	       free_clientInfo(&h, 0);
	  }
     
     debug("proxy", 0, "MMSBox: MM7 Shutting down...");
}

static void free_clientInfo(MmsHTTPClientInfo *h, int freeh)
{

     debug("free info", 0,
	   " entered free_clientinfo %d, ip=[%ld]", freeh, (long)h->ip);
     
     octstr_destroy(h->ip);
     octstr_destroy(h->url);    
     octstr_destroy(h->ua); 
     octstr_destroy(h->body);    
     http_destroy_cgiargs(h->cgivars);
     http_destroy_headers(h->headers);

     if (freeh) gw_free(h);    

     debug("free info", 0, " left free_clientinfo");  
}

/* XXX Returns msgid in mmsc or NULL if error. Caller uses this for DLR issues. 
 * Caller must make sure throughput issues
 * are observed!
 * Don't remove from queue on fail, just leave it to expire. 
 */
static Octstr *mm7soap_send(MmscGrp *mmc, Octstr *from, Octstr *to, 
			    Octstr *transid,
			    Octstr *linkedid, 
			    char *vasid,
			    Octstr *service_code,
			    List *hdrs,
			    MmsMsg *m, Octstr **error,
			    int *retry)
{
	Octstr *ret = NULL;
	int mtype = mms_messagetype(m);
	int hstatus = HTTP_OK, tstatus  = -1;
	List *xto = gwlist_create();
	MSoapMsg_t *mreq = NULL, *mresp = NULL;
	List *rh = NULL, *ph = NULL;
	Octstr *body = NULL, *rbody = NULL, *url = NULL; 
	Octstr *s, *distrib = NULL;

	// Avoid overwriting original to/from
	to = octstr_duplicate(to);
	from = octstr_duplicate(from);
	apply_number_substitutions(&to, mmc->outgoing_substitution_patterns);
	apply_number_substitutions(&from, mmc->outgoing_substitution_patterns);
     
	info(0, "MMSBox: Send[soap] to MMSC[%s], msg type [%s], from %s, to %s", 
		mmc->id ? octstr_get_cstr(mmc->id) : "", 
		mms_message_type_to_cstr(mtype), 
		octstr_get_cstr(from), octstr_get_cstr(to)); 
     
	gwlist_append(xto, to);
     
	if (hdrs)
		distrib = http_header_value(hdrs, octstr_imm("X-Mbuni-DistributionIndicator"));
	if ((mreq = mm7_mmsmsg_to_soap(m, (mmc == NULL || mmc->no_senderaddress == 0) ? from : NULL, 
		xto, transid,
		service_code, 
		linkedid, 
		1, octstr_get_cstr(mmc->vasp_id), vasid, NULL, 0,/* UA N/A on this side. */
		distrib)) == NULL) {
			*error = octstr_format("Failed to convert Msg[%S] 2 SOAP message!",
				mms_message_type_to_string(mtype));
			goto done1;
		}
     
	if (mm7_soapmsg_to_httpmsg(mreq, &mmc->ver, &rh, &body) < 0) {
		*error = octstr_format("Failed to convert SOAP message to HTTP Msg!");
		goto done1;
	} 

	if (hdrs)
		http_header_combine(rh, hdrs);  /* If specified, then update and pass on. */
	
	hstatus = mmsbox_url_fetch_content(HTTP_METHOD_POST, mmc->mmsc_url, rh, body, &ph,&rbody);
	if (http_status_class(hstatus) != HTTP_STATUS_SUCCESSFUL) {
		*error = octstr_format("Failed to contact MMC[url=%S] => HTTP returned status=[%d]!",
			mmc->mmsc_url, hstatus);
		goto done1;
	}
     
	if ((mresp = mm7_parse_soap(ph, rbody)) == NULL) {
		*error = octstr_format("Failed to parse MMSC[url=%S, id=%S]  response!",
			mmc->mmsc_url,  mmc->id);
		goto done1;
	}
     
	/* Now look at response code and use it to tell you what you want. */
	if ((s = mm7_soap_header_value(mresp, octstr_imm("StatusCode"))) != NULL) {
		tstatus = atoi(octstr_get_cstr(s));
		octstr_destroy(s);
	} else if ((s = mm7_soap_header_value(mresp, octstr_imm("faultstring"))) != NULL) {
		tstatus = atoi(octstr_get_cstr(s));
		octstr_destroy(s);
	} else
		tstatus = MM7_SOAP_FORMAT_CORRUPT; 
     
	if (!MM7_SOAP_STATUS_OK(tstatus) && tstatus != MM7_SOAP_COMMAND_REJECTED) {
		Octstr *detail =  mm7_soap_header_value(mresp, octstr_imm("Details"));
		char *tmp = (char *)mms_soap_status_to_cstr(tstatus);
		if (detail == NULL)
			detail = mm7_soap_header_value(mresp, octstr_imm("faultcode"));
		ret = NULL;
		info(0, "Send to MMSC[%s], failed, code=[%d=>%s], detail=[%s]", 
			mmc ? octstr_get_cstr(mmc->id) : "", 
			tstatus, tmp ? tmp : "", 
			detail ? octstr_get_cstr(detail) : "");

		*error = octstr_format("Failed to deliver to MMC[url=%S, id=%S], status=[%d=>%s]!",
			mmc->mmsc_url, 
			mmc->id,
			tstatus, 
			tmp ? tmp : "");
			
		octstr_destroy(detail);	  
	} else {	  
		ret = mm7_soap_header_value(mresp, octstr_imm("MessageID"));	  
		info(0, "Sent to MMC[%s], code=[%d=>%s], msgid=[%s]", octstr_get_cstr(mmc->id), 
			tstatus, mms_soap_status_to_cstr(tstatus), ret ? octstr_get_cstr(ret) : "(none)");
	}

	if (ret)
		mms_log2("Sent", from, to, -1, ret, NULL, mmc->id, "MMSBox", NULL, NULL);
	else
		mms_log2("Sending failed", from, to, -1, 0, NULL, mmc->id, "MMSBox", NULL, NULL);
done1:
	*retry = (ret == NULL && (!MM7_SOAP_CLIENT_ERROR(tstatus) || tstatus < 0));
     
    mm7_soap_destroy(mreq);
    mm7_soap_destroy(mresp);	  
    http_destroy_headers(rh);
    octstr_destroy(body);
    http_destroy_headers(ph);
    octstr_destroy(rbody);
    octstr_destroy(url);
    octstr_destroy(distrib);
    octstr_destroy(to);
	octstr_destroy(from);
    gwlist_destroy(xto, NULL);

    return ret;
}

static Octstr *mm7soap_mpes_send(MmscGrp *mmc, Octstr *from, Octstr *to, 
			    Octstr *transid,
			    Octstr *linkedid, 
			    char *vasid,
			    Octstr *service_code,
			    List *hdrs,
			    MmsMsg *m, Octstr **error,
			    int *retry)
{
	Octstr *ret = NULL;
	int mtype = mms_messagetype(m);
	int hstatus = HTTP_OK, tstatus  = -1;
	List *xto = gwlist_create();
	MSoapMsg_t *mreq = NULL, *mresp = NULL;
	List *rh = NULL, *ph = NULL;
	Octstr *body = NULL, *rbody = NULL, *url = NULL; 
	Octstr *s, *distrib = NULL;
	HTTPCaller *http_caller = http_caller_create();

	// Avoid overwriting original to/from
	to = octstr_duplicate(to);
	from = octstr_duplicate(from);
	apply_number_substitutions(&to, mmc->outgoing_substitution_patterns);
	apply_number_substitutions(&from, mmc->outgoing_substitution_patterns);
     
	info(0, "MMSBox: Send[mpes/soap] to MMSC[%s], msg type [%s], from %s, to %s",
		mmc->id ? octstr_get_cstr(mmc->id) : "",
		mms_message_type_to_cstr(mtype), 
		octstr_get_cstr(from), octstr_get_cstr(to)); 
     
	gwlist_append(xto, to);
     
	if (hdrs)
		distrib = http_header_value(hdrs, octstr_imm("X-Mbuni-DistributionIndicator"));
	if ((mreq = mm7_mmsmsg_to_soap(m, (mmc == NULL || mmc->no_senderaddress == 0) ? from : NULL, 
		xto, transid,
		service_code, 
		linkedid, 
		1, octstr_get_cstr(mmc->vasp_id), vasid, NULL, 0,/* UA N/A on this side. */
		distrib)) == NULL) {
			*error = octstr_format("Failed to convert Msg[%S] 2 SOAP message!",
				mms_message_type_to_string(mtype));
			goto done1;
		}
     
	if (mm7_soapmsg_to_httpmsg(mreq, &mmc->ver, &rh, &body) < 0) {
		*error = octstr_format("Failed to convert SOAP message to HTTP Msg!");
		goto done1;
	} 

	if (hdrs)
		http_header_combine(rh, hdrs);  /* If specified, then update and pass on. */
		
	//hstatus = mmsbox_url_fetch_content(HTTP_METHOD_POST, mmc->mmsc_url, rh, body, &ph,&rbody);
	hstatus = mms_url_fetch_content_with_basic_auth(HTTP_METHOD_POST, mmc->mmsc_url, rh, body, &ph, &rbody);
	http_caller_destroy(http_caller);
	
	if (http_status_class(hstatus) != HTTP_STATUS_SUCCESSFUL) {
		*error = octstr_format("Failed to contact MMC[url=%S] => HTTP returned status=[%d]!",
			mmc->mmsc_url, hstatus);
	}
     
	if ((mresp = mm7_parse_soap(ph, rbody)) == NULL) {
		*error = octstr_format("Failed to parse MMSC[url=%S, id=%S]  response!",
			mmc->mmsc_url,  mmc->id);
		goto done1;
	}
     
	/* Now look at response code and use it to tell you what you want. */
	if ((s = mm7_soap_header_value(mresp, octstr_imm("StatusCode"))) != NULL) {
		debug("", 0, "MPES/SOAP status code: %s", octstr_get_cstr(s));
		tstatus = atoi(octstr_get_cstr(s));
		octstr_destroy(s);
	} else 
		tstatus = MM7_SOAP_FORMAT_CORRUPT;	
	
	if ((s = mm7_soap_header_value(mresp, octstr_imm("faultstring"))) != NULL) {
		//tstatus = atoi(octstr_get_cstr(s));
		//octstr_destroy(s);
	} else
		tstatus = MM7_SOAP_FORMAT_CORRUPT;
		
	if (http_status_class(hstatus) != HTTP_STATUS_SUCCESSFUL)
		goto done1;
		
	if (!MM7_SOAP_STATUS_OK(tstatus) && tstatus != MM7_SOAP_COMMAND_REJECTED) {
		Octstr *detail =  mm7_soap_header_value(mresp, octstr_imm("Details"));
		char *tmp = (char *)mms_soap_status_to_cstr(tstatus);
		if (detail == NULL)
			detail = mm7_soap_header_value(mresp, octstr_imm("faultcode"));
		ret = NULL;
		info(0, "Send to MMSC[%s], failed, code=[%d=>%s], detail=[%s]", 
			mmc ? octstr_get_cstr(mmc->id) : "", 
			tstatus, tmp ? tmp : "", 
			detail ? octstr_get_cstr(detail) : "");

		*error = octstr_format("Failed to deliver to MMC[url=%S, id=%S], status=[%d=>%s]!",
			mmc->mmsc_url, 
			mmc->id,
			tstatus, 
			tmp ? tmp : "");
			
		octstr_destroy(detail);	  
	} else {	  
		ret = mm7_soap_header_value(mresp, octstr_imm("MessageID"));	  
		info(0, "Sent to MMC[%s], code=[%d=>%s], msgid=[%s]", octstr_get_cstr(mmc->id), 
			tstatus, mms_soap_status_to_cstr(tstatus), ret ? octstr_get_cstr(ret) : "(none)");
	}

	if (ret)
		mms_log2("Sent", from, to, -1, ret, NULL, mmc->id, "MMSBox", NULL, NULL);
	else
		mms_log2("Sending failed", from, to, -1, 0, NULL, mmc->id, "MMSBox", NULL, NULL);
done1:
	*retry = (ret == NULL && (!MM7_SOAP_CLIENT_ERROR(tstatus) || tstatus < 0));
     
    mm7_soap_destroy(mreq);
    mm7_soap_destroy(mresp);	  
    http_destroy_headers(rh);
    octstr_destroy(body);
    http_destroy_headers(ph);
    octstr_destroy(rbody);
    octstr_destroy(url);
    octstr_destroy(distrib);
    octstr_destroy(to);
	octstr_destroy(from);
    gwlist_destroy(xto, NULL);

    return ret;
}

static Octstr  *mm7eaif_send(MmscGrp *mmc, Octstr *from, Octstr *to, 
			     Octstr *transid,
			     char *vasid,
			     List *hdrs,
			     MmsMsg *m, Octstr **error,
			     int *retry)
{
     Octstr *ret = NULL, *resp = NULL;
     int mtype = mms_messagetype(m);
     int hstatus = HTTP_OK;
     List *rh = http_create_empty_headers(), *ph = NULL;
     Octstr *body = NULL, *rbody = NULL, *url = NULL, *xver;
     char *msgtype;

	 // Avoid overwriting original to/from
	 to = octstr_duplicate(to);
	 from = octstr_duplicate(from);
	 apply_number_substitutions(&to, mmc->outgoing_substitution_patterns);
     apply_number_substitutions(&from, mmc->outgoing_substitution_patterns);
     
     info(0, "MMSBox: Send [eaif] to MMC[%s], msg type [%s], from %s, to %s", 
	  mmc ? octstr_get_cstr(mmc->id) : "", 
	  mms_message_type_to_cstr(mtype), 
	  octstr_get_cstr(from), octstr_get_cstr(to));

     http_header_remove_all(rh, "X-Mms-Allow-Adaptations");
     
	 //http_header_add(rh, "X-NOKIA-MMSC-To", octstr_get_cstr(to));
     //http_header_add(rh, "X-NOKIA-MMSC-From", octstr_get_cstr(from));

     xver = octstr_format(EAIF_VERSION, mmc->ver.major, mmc->ver.minor1);
     //http_header_add(rh, "X-NOKIA-MMSC-Version", octstr_get_cstr(xver));
     octstr_destroy(xver);

     if (mtype == MMS_MSGTYPE_SEND_REQ || mtype == MMS_MSGTYPE_RETRIEVE_CONF) {
	/*
		int i;
		int padding_length = 32 - octstr_len(mms_msg_id(m));
		Octstr *nokia_msg_id = octstr_duplicate(mms_msg_id(m));
		Octstr *padding = octstr_create("");
		for (i = 0; i < padding_length; i++)
			octstr_append_char(padding, '0');
		octstr_append(nokia_msg_id, padding);
		http_header_add(rh, "X-Nokia-MMSC-Message-Id", octstr_get_cstr(nokia_msg_id));
		octstr_destroy(padding);
		octstr_destroy(nokia_msg_id);
	*/	
	  	msgtype = "MultiMediaMessage";
	  	mms_make_sendreq(m); /* ensure it is a sendreq. */
     } else if (mtype == MMS_MSGTYPE_DELIVERY_IND)
	  msgtype = "DeliveryReport";
     else
	  msgtype = "ReadReply";
     //http_header_add(rh, "X-NOKIA-MMSC-Message-Type", msgtype);

     if (hdrs)
	  http_header_combine(rh, hdrs);  /* If specified, then update and pass on. */

     http_header_add(rh, "Content-Type", "application/vnd.wap.mms-message");

     /* Patch the message FROM and TO fields. */
     mms_replace_header_value(m, "From", octstr_get_cstr(from));
     mms_replace_header_value(m, "To", octstr_get_cstr(to));
     mms_replace_header_value(m,"X-Mms-Transaction-ID",
			      transid ? octstr_get_cstr(transid) : "000");
     body = mms_tobinary(m);

	/*
	// This is a debug print, currently disabled. 
	// It is practical for viewing _exactly_ what is sent.
	hex_body = octstr_duplicate(body);
	octstr_binary_to_hex(hex_body,1);
	debug("mm7eaif_send",0,"Body plan: [%s]", octstr_get_cstr(body));	
	debug("mm7eaif_send",0,"Body as hex: [%s]", octstr_get_cstr(hex_body));
	*/

     hstatus = mmsbox_url_fetch_content(HTTP_METHOD_POST, mmc->mmsc_url, rh, body, &ph, &rbody);

     if (http_status_class(hstatus) != HTTP_STATUS_SUCCESSFUL) {
	  *error = octstr_format("Failed to contact MMC[url=%S] => HTTP returned status = %d !",
				 mmc->mmsc_url, hstatus);
     } else {
	  MmsMsg *mresp = rbody ? mms_frombinary(rbody, octstr_imm("anon@anon")) : NULL;
	
      debug("",0, "DUMPING RBODY:");
  	  octstr_dump(rbody,0); // DEBUG
	  
	  resp = octstr_imm("Ok");
	  if (mresp && mms_messagetype(mresp) == MMS_MSGTYPE_SEND_CONF)
	       resp = mms_get_header_value(mresp, octstr_imm("X-Mms-Response-Status"));
	  if (octstr_case_compare(resp, octstr_imm("ok")) != 0)
	       hstatus = HTTP_STATUS_SERVER_ERROR; /* error. */
	  else if (mresp)
	       ret = mms_get_header_value(mresp, octstr_imm("Message-ID"));

	  mms_destroy(mresp);
     }

     if (hstatus < 0)
	  ret = NULL;
     else {
	  hstatus = http_status_class(hstatus);	  
	  if (hstatus == HTTP_STATUS_SERVER_ERROR ||
	      hstatus == HTTP_STATUS_CLIENT_ERROR)
	       ret = NULL;
	  else if (!ret) 
	       ret = http_header_value(ph, octstr_imm("X-Nokia-MMSC-Message-Id"));
     }

	debug("",0,"ret is %s", ret ? octstr_get_cstr(ret) : "(null)");

     *retry = (ret == NULL && (hstatus == HTTP_STATUS_SERVER_ERROR || hstatus < 0));

     if (ret)
	  mms_log2("Sent", from, to, -1, ret, NULL, mmc->id, "MMSBox", NULL, NULL);
     else
          mms_log2("Sending failed", from, to, -1, 0, NULL, mmc->id, "MMSBox", NULL, NULL);

#if 0
     info(0, "Sent to MMC[%s], code=[%d], resp=[%s] msgid [%s]", 
	  octstr_get_cstr(mmc->id), 
	  hstatus, resp ? octstr_get_cstr(resp) : "(none)", ret ? octstr_get_cstr(ret) : "(none)");
#endif 

     http_destroy_headers(rh);
     octstr_destroy(body);
     http_destroy_headers(ph);
     octstr_destroy(rbody);
     octstr_destroy(url);
     octstr_destroy(resp);
     octstr_destroy(to);
	 octstr_destroy(from);
     return ret;
}

static int mms_sendtommsc(MmscGrp *mmc, Octstr *from, Octstr *to, Octstr *transid, 
			  Octstr *orig_transid,
			  Octstr *linkedid, char *vasid, Octstr *service_code,
			  MmsMsg *m, Octstr *dlr_url, Octstr *rr_url,
			  List *hdrs, 
			  Octstr **new_msgid,
			  Octstr **err) 
{
     Octstr *id = NULL, *groupid = NULL;
     int ret = 0, retry  = 0;
     double throughput = 0;
     
     mutex_lock(mmc->mutex); { /* Grab a lock on it. */
	  if (mmc->type == SOAP_MMSC)
	       id = mm7soap_send(mmc, from, to, transid, linkedid, vasid, service_code, hdrs, m, err, &retry);
	  else if (mmc->type == EAIF_MMSC) {
	       id = mm7eaif_send(mmc, from, to, transid, vasid, hdrs, m, err, &retry);
	       if (id)
			debug("", 0, "mms_sendtommsc: got id [%s] from mm7eaif_send.", octstr_get_cstr(id));
		   else
		    debug("", 0, "mms_sendtommsc: didn't get valid id from mm7eaif_send.");
	  } else if (mmc->type == MPES_MMSC) {
	       id = mm7soap_mpes_send(mmc, from, to, transid, linkedid, vasid, service_code, hdrs, m, err, &retry);		
	  } else if (mmc->type == CUSTOM_MMSC && mmc->custom_started) 
	       id = mmc->fns->send_msg(mmc->data, 
				       from, to, transid, linkedid, vasid, 
				       service_code, m, hdrs, err, &retry);
	  else
	       error(0, "MMC[%s] of unknown type, can't send!",
		     mmc->id ? octstr_get_cstr(mmc->id) : "");
	
	  throughput = mmc->throughput;
	  groupid = mmc->group_id ? octstr_duplicate(mmc->group_id) : NULL;
	  
     }  mutex_unlock(mmc->mutex);  /* release lock */

     if (id) {
	  if (dlr_url)  /* remember the url's for reporting purposes. */
	       mms_dlr_url_put(id, "delivery-report", groupid, dlr_url, orig_transid);
	  if (rr_url)
	       mms_dlr_url_put(id, "read-report", groupid, rr_url, orig_transid);	  
	  ret = MMS_SEND_OK;
     } else 
	  ret = retry ? MMS_SEND_ERROR_TRANSIENT : MMS_SEND_ERROR_FATAL;
     
     *new_msgid = id;

     octstr_destroy(groupid);
     if (throughput > 0) 
	  gwthread_sleep(1.0/throughput);
     return ret;
}



static int sendMsg(MmsEnvelope *e)
{
	MmsMsg *msg = NULL;
	int i, n, ret = 1;
	Octstr *otransid = e->hdrs ? http_header_value(e->hdrs, octstr_imm("X-Mbuni-TransactionID")) : NULL;

	if ((msg = qfs->mms_queue_getdata(e)) == NULL)  {
		error(0, "MMSBox queue error: Failed to load message for queue id [%s]!", e->xqfname);
		goto done2;
	}

	for (i = 0, n = gwlist_len(e->to); i<n; i++) {
		int res = MMS_SEND_OK;
		MmsEnvelopeTo *to = gwlist_get(e->to, i);
		Octstr *err = NULL;
		time_t tnow = time(NULL);
		MmscGrp *mmc = NULL;
		Octstr *new_msgid = NULL;

		if (!to || !to->process)
			continue;

		/* Handle message expiry. */
		if (e->expiryt != 0 &&  e->expiryt < tnow) {
			err = octstr_format("MMSC error: Message expired while sending to %S!", to->rcpt);
			res = MMS_SEND_ERROR_FATAL;

			goto done;
		} else if (e->attempts >= mmsbox_maxsendattempts) {
			err = octstr_format("MMSBox error: Failed to deliver to "
				"%S after %ld attempts. (max attempts allowed is %ld)!", 
				to->rcpt, e->attempts, 
				mmsbox_maxsendattempts);
			res = MMS_SEND_ERROR_FATAL;
			goto done;
		}

		if ((mmc = get_handler_mmc(e->viaproxy, to->rcpt, e->from)) == NULL) {
			err = octstr_format("MMSBox error: Failed to deliver to "
				"%S. Don't know how to route!",
				to->rcpt);
			res = MMS_SEND_ERROR_TRANSIENT;
			goto done;
		}

		res = mms_sendtommsc(mmc, e->from, to->rcpt, 
			e->msgId,
			otransid,
			e->token, /* token = linkedid */
			e->vasid ? octstr_get_cstr(e->vasid) : NULL,
			e->vaspid,
			msg, 
			e->url1, e->url2,
			e->hdrs,
			&new_msgid,
			&err);

		done:
		
		debug("",0, "mms_sendtommsc returned: %d",res);
		
		/* The message was sent to the recipient */
		if (res == MMS_SEND_OK || res == MMS_SEND_QUEUED) {
			counter_increase(mms_sent_counter);			
			to->process = 0;
			mmsbox_send_report(to->rcpt, "delivery-report", e->url1,
				octstr_imm("Sent"), new_msgid, 
				mmc->id, mmc->group_id, otransid, NULL, -1);
				
		/* On FATAL errors  - Send rejected if there was a route 
		 * set to->process = 0 to ensure that the message is sent to this recipient again
		*/
		} else if (res == MMS_SEND_ERROR_FATAL && mmc)
			mmsbox_send_report(to->rcpt, "delivery-report",
			e->url1,
			(e->expiryt != 0 && e->expiryt < tnow) ? 
			octstr_imm("Expired") : octstr_imm("Rejected"),
			e->msgId, mmc->id, mmc->group_id, otransid, NULL, -1);
		
		if (res == MMS_SEND_ERROR_FATAL)
			to->process = 0; /* No more attempts. */

		/* Report what happened while trying to send this message */
		if (err == NULL)
			info(0, "%s MMSBox Outgoing Queue MMS Send: From %s, to %s, msgsize=%ld: msgid=[%s]", 
			SEND_ERROR_STR(res),
			octstr_get_cstr(e->from), octstr_get_cstr(to->rcpt), e->msize,
			new_msgid ? octstr_get_cstr(new_msgid) : "NULL");
		else {
			error(0, "%s MMSBox Outgoing Queue MMS Send: From %s, to %s, msgsize=%ld: %s",
			SEND_ERROR_STR(res),
			octstr_get_cstr(e->from), octstr_get_cstr(to->rcpt), e->msize, octstr_get_cstr(err));
		}

		octstr_destroy(new_msgid);
		octstr_destroy(err);

		e->lasttry = tnow;
		
		/*
		 * Queue update will update the queue (setting a recipient as processed)
		 * or delete the queue entry if all recipients have been processed.
		 */
		ret = qfs->mms_queue_update(e);
		if (ret != 0)  /* Queue entry gone (deleted) */
			break;
	}

	done2:
	mms_destroy(msg);
	octstr_destroy(otransid);

	if (ret == 0) {
		/* Update the queue if it is still valid (e.g. recipients not handled) 
		* XXX can this happen here??...
		*/
		e->lasttry = time(NULL);
		e->attempts++;  /* Update count of number of delivery attempts. */   
		e->sendt = e->lasttry + mmsbox_send_back_off * e->attempts;

		/* Upddate queue entry */
		ret = qfs->mms_queue_update(e);
	}
	
	// Ret will be: 
	// -1 on error (e.g. envelope couldn't be written)
	// 0 on success
	// 1 if the envelope was discard because of either: no more recipients or fatal error
	return ret;
}

void mmsbox_outgoing_queue_runner(int *rstop)
{
     qfs->mms_queue_run(octstr_get_cstr(outgoing_qdir), 
		   sendMsg, queue_interval, maxthreads, rstop);
}

void mmsbox_response_queue_runner(int *rstop)
{
   List *rh, *rph;
   Octstr *rb;
   eaif_pending_async_response = gwlist_create();
	
		
	MmsMsgRef *r = NULL;
	int status;
	
	/* It seems a bit artificial to add a producer in this (which is a consumer thread), but we know
	  that when rstop becomes false, there are no more producers and we need to add at least one or consume wont sleep */
	gwlist_add_producer(eaif_pending_async_response);
	
	info(0, "Starting mmsbox_response_queue_runner");
	while (!*rstop) {
		r = gwlist_consume(eaif_pending_async_response);
		
		gw_assert(r);
		gw_assert(r->msgid);
		gw_assert(r->url);
		
		debug("", 0, "Sending asynchronous response to message with id <%s> to <%s>", octstr_get_cstr(r->msgid), octstr_get_cstr(r->url));
		
		
		rh = http_create_empty_headers();
		
		http_header_add(rh, "X-NOKIA-MMSC-Message-Id", octstr_get_cstr(r->msgid));
		http_header_add(rh, "X-NOKIA-MMSC-Message-Type", "MultiMediaMessage");
		http_header_add(rh, "X-NOKIA-MMSC-Status", "200");

		// We dont really care much.. There is no retrying or anything like that. But in general we do not expect to see this 
		// message unless something goes wrong
		if (HTTP_NO_CONTENT != (status = mms_url_fetch_content(HTTP_METHOD_POST, r->url, rh, octstr_imm(""), &rph, &rb)))
			warning(0, "Got unexpected response code <%d> to message with msgid %s", status, octstr_get_cstr(r->msgid));
		else
			info(0, "Sent asynchronous reply on msgid <%s> to <%s>", octstr_get_cstr(r->msgid), octstr_get_cstr(r->url));
			
		octstr_destroy(rb);
	    http_destroy_headers(rph);
	    http_destroy_headers(rh);
	}
	
	info(0, "Shutting down mmsbox_response_queue_runner thread");
	
	gwlist_remove_producer(eaif_pending_async_response);
	gwlist_destroy(eaif_pending_async_response, NULL);
}

/* A dict of "seen" message id's for  is maintained each mmsc
 * This thread/function removes messagges from this dics when they expire
*/
void clean_duplicate_list(int *rstop)
{
	Octstr *msgid;
	int i, j, removed_count, left_count;
	int msgref_age;
	
	while(!*rstop) {
		gwthread_sleep(60); // Go to sleep until next cleanup
		
		if (*rstop) // Make sure we stop immediately
			break;
		
		removed_count=left_count=0;
		info(0, "Cleaning up msgid duplicate detection lists");
		
		for (i = 0; i < gwlist_len(mmscs); i++) {
			MmscGrp* mmsc = gwlist_get(mmscs, i);
			
			if (mmsc->seen_messages) {
				List *seen_msg_keys = dict_keys(mmsc->seen_messages);
				for (j = 0; j < gwlist_len(seen_msg_keys); j++) {
					msgid = gwlist_get(seen_msg_keys, j);
					
					MmsMsgRef* msgref = dict_get(mmsc->seen_messages, msgid);
					
					gw_assert(msgref);
					
					//msgref_dump(msgref);
		
					msgref_age = time(NULL) - msgref->timestamp;
					//debug("", 0, "msgref_age=%d, msgref->timestamp <%ld> mmsc->msgid_expire <%ld>", msgref_age, msgref->timestamp, mmsc->msgid_expire);
					if (msgref_age > mmsc->msgid_expire) {
						debug("", 0, "Removing msgref with msgid <%s>", octstr_get_cstr(msgid));
						dict_remove(mmsc->seen_messages, msgid);
						msgref_destroy(msgref);
						removed_count++;
					} else
						left_count++;
				}
				gwlist_destroy(seen_msg_keys,NULL);
			}
		}
		info(0, "Cleaned up msgid duplicate detection lists. %d msgid's removed. %d msgid's left in list.", removed_count, left_count);
	}
}