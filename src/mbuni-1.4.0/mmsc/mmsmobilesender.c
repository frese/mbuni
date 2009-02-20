/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMS client sender: notifications/reports to clients via WAP Push, 
 * manages out-going messages.
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */
#include "mmsrelay.h"
#include <errno.h>
#include <strings.h>

#define WAPPUSH_PORT 2948


static HTTPCaller *httpcaller; 
static MmsEnvelope edummy;


static MmsEnvelope *update_env_success(MmsEnvelope *env, MmsEnvelopeTo *xto)
{
     time_t tnow = time(NULL);
     
     if (xto && !(env->msgtype == MMS_MSGTYPE_SEND_REQ || 
		  env->msgtype == MMS_MSGTYPE_RETRIEVE_CONF))
	  xto->process = 0; /* No more processing. */
     else {
	  env->lasttry = tnow;
	  env->attempts++;     
	  
	  /* If max send attempts has been reached, set next try to expiry time, otherwise 
	   * use normal back-off procedure 
	   */
	  if (env->attempts >= settings->maxsendattempts)
	       env->sendt = env->expiryt;
	  else
	       env->sendt = env->lasttry + settings->send_back_off * env->attempts;
     }
     
     if (settings->qfs->mms_queue_update(env) == 1)
	  env = NULL;     
     return env;
} 

static MmsEnvelope *update_env_failed(MmsEnvelope *env)
{
     time_t tnow = time(NULL);
     if (env && env != &edummy) {
	  env->sendt = tnow + settings->send_back_off;
	  env->lasttry = tnow;
	  
	  if (settings->qfs->mms_queue_update(env) == 1)
	       env = NULL;
     }
     return env;
}

static void start_push(Octstr *rcpt_to, int isphonenum, MmsEnvelope *e, MmsMsg *msg)
{
     List *pheaders;
     static unsigned char ct; /* Transaction counter -- do we need it? */
     Octstr *to = NULL;
     
     Octstr *pduhdr = octstr_create("");
     
     Octstr *s = NULL;
     

     info(0, "mms2mobile.startpush: notification to %s\n", octstr_get_cstr(rcpt_to));
     
     if (!rcpt_to) {
	  error(0, "mobilesender: Queue entry %s has no recipient address!", e->xqfname);
	  goto done;
     } else
	  to = octstr_duplicate(rcpt_to);
     

     ct++;    
     octstr_append_char(pduhdr, ct);  /* Pushd id */
     octstr_append_char(pduhdr, 0x06); /* PUSH */

     
#if 1
     octstr_append_char(pduhdr, 1 + 1 + 1);
     octstr_append_char(pduhdr, 0xbe); /* content type. */     
#else
     octstr_append_char(pduhdr, 
			1 + 1 + sizeof "application/vnd.wap.mms-message"); /*header length. */     
     octstr_append_cstr(pduhdr, "application/vnd.wap.mms-message");
     octstr_append_char(pduhdr, 0x0); /* string terminator. */
#endif
     octstr_append_char(pduhdr, 0xaf); /* push application ID header and value follows. */
     octstr_append_char(pduhdr, 0x84); /* ... */

     s = mms_tobinary(msg);

     if (isphonenum) {
	  Octstr *url;
	  
	  octstr_url_encode(to);
	  octstr_url_encode(s);
#if 0
	  octstr_dump(pduhdr, 0);
#endif

	  octstr_url_encode(pduhdr);
	  
	  url = octstr_format("%S&text=%S%S&to=%S&udh=%%06%%05%%04%%0B%%84%%23%%F0",	
			      settings->sendsms_url, pduhdr, s, to);     
	  
	  pheaders = http_create_empty_headers();
	  http_header_add(pheaders, "Connection", "close");
	  http_header_add(pheaders, "User-Agent", MM_NAME "/" MMSC_VERSION);	       
	  
	  http_start_request(httpcaller, HTTP_METHOD_GET, url, 
			     pheaders, NULL, 0, e, NULL);
	  	  
	  http_destroy_headers(pheaders);
	  octstr_destroy(url);
     } else { /* An IP Address: Send packet, forget. */
	  Octstr *addr = udp_create_address(to, WAPPUSH_PORT);
	  int sock = udp_client_socket();
	  
	  if (sock > 0) {
	       MmsEnvelopeTo *xto = gwlist_get(e->to,0);
	       octstr_append(pduhdr, s);
#if 0
	       octstr_dump(pduhdr, 0);
#endif
	       udp_sendto(sock, pduhdr, addr);
	       close(sock); /* ?? */      
	       mms_log2("Notify", octstr_imm("system"), to, 
			-1, e ? e->msgId : NULL, 
			NULL, NULL, "MM1", NULL,NULL);
	       e = update_env_success(e, xto);
	  } else {
	       e = update_env_failed(e);
	       error(0, "push to %s:%d failed: %s", octstr_get_cstr(to), WAPPUSH_PORT, strerror(errno));
	  }
	  octstr_destroy(addr);
	  if (e) 
	       settings->qfs->mms_queue_free_env(e);
     }
 done:
     octstr_destroy(to);
     octstr_destroy(pduhdr);
     octstr_destroy(s);
}


static int receive_push_reply(HTTPCaller *caller)
{
     int http_status;
     List *reply_headers = NULL;
     Octstr *final_url = NULL, *reply_body = NULL;

     MmsEnvelope *env;
     
     http_status = HTTP_UNAUTHORIZED;
     
     while ((env = http_receive_result_real(caller, &http_status, &final_url, &reply_headers,
					    &reply_body,1)) != NULL) {
	  MmsEnvelopeTo *xto = NULL;
	  Octstr *to = NULL;
	  	  
	  if (http_status == -1 || final_url == NULL) {
	       error(0, "push failed, no reason found");
	       goto push_failed;
	  } 
	  
	  if (env == &edummy) /* Skip this one it is a dummy. */
	       goto push_free_env;
	  xto = gwlist_get(env->to, 0);
	  if (xto)
	       to = xto->rcpt;
	  else {
	       error(0, "mobilesender: Queue entry %s has no recipient address!", env->xqfname);
	       goto push_failed;
	  }
	  
	  info(0, "send2mobile.push_reply[%s]: From %s, to %s =>  %d", 
	       env->xqfname,
	       octstr_get_cstr(env->from), octstr_get_cstr(to), http_status);
	  
	  if (http_status == HTTP_UNAUTHORIZED || 
	      http_status == HTTP_NOT_FOUND ||
	      http_status == HTTP_FORBIDDEN) { /* This is a temporary system error
						* do not increase attempts, count, 
						* merely reschedule 
						* for a minute or so later. 
						*/ 
	       
	       error(0, "Deffered notification, WAP Push failed for "
		     "msgid %s to %s, http error: %d!", octstr_get_cstr(env->msgId), 
		     octstr_get_cstr(to), http_status); 
	       goto push_failed;
	  }
	  

	  debug("mobilesender.push", 0, "Push reply headers were");
	  http_header_dump(reply_headers);
	  
	  mms_log2("Notify", octstr_imm("system"), to, 
		   -1, env ? env->msgId : NULL, NULL, NULL, "MM1", NULL,NULL);

	  if ((env = update_env_success(env, xto)) != NULL)
	       goto push_free_env;

	  /* Fall through. */
     push_failed:    
	  env = update_env_failed(env);
     push_free_env:
	  if (env && env != &edummy) 
	       settings->qfs->mms_queue_free_env(env);
	  
	  octstr_destroy(final_url);    
	  octstr_destroy(reply_body);
	  http_destroy_headers(reply_headers);	  
     }

    return 0;        
}

static int sendNotify(MmsEnvelope *e)
{
     Octstr *to;
     MmsMsg *msg, *smsg = NULL;
     MmsEnvelopeTo *xto = gwlist_get(e->to, 0);
     Octstr *err = NULL;
     time_t tnow = time(NULL);
     int j, k, len;
     Octstr *phonenum = NULL, *rcpt_ip = NULL, *msgId, *from, *fromproxy;
     int mtype, msize;
     int res = MMS_SEND_OK, dlr;
     time_t expiryt;
     char *prov_notify_event = NULL;
     char *rtype = NULL;

          
     if (e->lastaccess != 0) { /* This message has been fetched at least once, no more signals. */	  
	  e->sendt = e->expiryt + 3600*24*30*12;
	  info(0, "MM1: Message [ID: %s] fetched/touched at least once. Skipping",
	       e->xqfname);
	  return settings->qfs->mms_queue_update(e);
     }

     if (!xto) {
	  error(0, "mobilesender: Queue entry %s with no recipients!", 
		e->xqfname);
	  return 0;
     }

     msg = settings->qfs->mms_queue_getdata(e);
     to = octstr_duplicate(xto->rcpt);
     expiryt = e->expiryt;
     msgId = e->msgId ? octstr_duplicate(e->msgId) : NULL;
     from = octstr_duplicate(e->from);
     fromproxy = e->fromproxy ? octstr_duplicate(e->fromproxy) : NULL;
     msize = e->msize;
     dlr = e->dlr;

     if (e->expiryt != 0 &&  /* Handle message expiry. */
	 e->expiryt < tnow) {
	  err = octstr_format("MMSC error: Message expired while sending to %S!", to);
	  res = MMS_SEND_ERROR_FATAL;
	  prov_notify_event = "failedfetch";
	  rtype = "Expired";	  
	  goto done;
     } else if (e->attempts >= settings->maxsendattempts) {
	  err = octstr_format("MMSC error: Failed to deliver to %S after %ld attempts!", 
			      to, e->attempts);
	  res = MMS_SEND_ERROR_FATAL;

	  prov_notify_event = "failedfetch";
	  rtype = "Expired";	  
	  goto done;
     }
     
     j = octstr_case_search(to, octstr_imm("/TYPE=PLMN"), 0);
     k = octstr_case_search(to, octstr_imm("/TYPE=IPv"), 0);
     len = octstr_len(to);
     
     if (j > 0 && j - 1 +  sizeof "/TYPE=PLMN" == len) { /* A proper number. */
	  phonenum = octstr_copy(to, 0, j);
#if 0
	  normalize_number(octstr_get_cstr(settings->unified_prefix), &phonenum);
#else
	  mms_normalize_phonenum(&phonenum, 
				 octstr_get_cstr(settings->unified_prefix), 
				 settings->strip_prefixes);	  
#endif
     }     else if (k > 0 && k + sizeof "/TYPE=IPv" == len) 
	  rcpt_ip = octstr_copy(to, 0, k);
     else {
	  /* We only handle phone numbers here. */
	  err = octstr_format("Unexpected recipient %s in MT queue!", octstr_get_cstr(to));	  
	  res = MMS_SEND_ERROR_FATAL;	  
	  goto done;
     }
     
     mtype = mms_messagetype(msg);     
     
     /* For phone, getting here means the message can be delivered. So: 
      * - Check whether the recipient is provisioned, if not, wait (script called will queue creation req)
      * - If the recipient can't take MMS, then send SMS.
      */
     
     /* We handle two types of requests: send and delivery/read notifications. 
      * other types of messages cannot possibly be in this queue!
      */
     
     if (mtype == MMS_MSGTYPE_SEND_REQ ||
	 mtype == MMS_MSGTYPE_RETRIEVE_CONF) {
	  Octstr *url, *transid;
	  
	  if (phonenum) {
	       int send_ind = mms_ind_send(settings->prov_getstatus, phonenum);
	       
	       if (send_ind < 0 && 
		   settings->notify_unprovisioned)
		    send_ind = 0;

	       if (send_ind < 0) { /* That is, recipient is not (yet) provisioned. */
		    res = MMS_SEND_ERROR_TRANSIENT;
		    err = octstr_format("%S is not provisioned for MMS reception, delivery deferred!", 
					phonenum);
		    
		    /* Do not increase delivery attempts counter. */
		    e->lasttry = tnow;		    
		    e->sendt = e->lasttry + settings->send_back_off * (1 + e->attempts);
		    
		    if (settings->qfs->mms_queue_update(e) == 1) 
			 e = NULL; /* Queue entry gone. */	
		    else 	  
			 settings->qfs->mms_queue_free_env(e);		    		    
		    goto done;
	       } else if (send_ind == 0) { /*  provisioned but does not support */		    
		    Octstr *s = octstr_format(octstr_get_cstr(settings->mms_notify_txt),
					      from);
		    if (settings->notify_unprovisioned && s && octstr_len(s) > 0) { /* Only send if the string was set. */
			 List *pheaders;					    
			 Octstr *sto = octstr_duplicate(phonenum);
			 
			 octstr_url_encode(s);
			 octstr_url_encode(sto);
			 
			 url = octstr_format("%S&text=%S&to=%S",settings->sendsms_url,s, sto);
			 pheaders = http_create_empty_headers();
			 http_header_add(pheaders, "Connection", "close");
			 http_header_add(pheaders, "User-Agent", MM_NAME "/" VERSION);      
			 
			 http_start_request(httpcaller, HTTP_METHOD_GET, url, 
					    pheaders, NULL, 0, &edummy, NULL);			 			 
			 http_destroy_headers(pheaders);
			 octstr_destroy(url);
			 octstr_destroy(sto);		   		 
		    } else if (s)
			 octstr_destroy(s);	
		    res = MMS_SEND_OK;
		    err = octstr_imm("No MMS Ind support, sent SMS instead");
		    
		    xto->process = 0; /* No more processing. */
		    if (settings->qfs->mms_queue_update(e) == 1) 
			 e = NULL; 
		    else 	  
			 settings->qfs->mms_queue_free_env(e);		    
		    goto done;

	       }	       
	  }
	  
	  /* To get here means we can send Ind. */
	  url = mms_makefetchurl(e->xqfname, e->token, MMS_LOC_MQUEUE,
				 phonenum ? phonenum : to,
				 settings);
	  info(0, "Preparing to notify client to fetch message at URL: %s",
	       octstr_get_cstr(url));
	  transid = mms_maketransid(e->xqfname, settings->host_alias);	  
	  
	  smsg = mms_notification(msg, e->msize, url, transid, 
				  e->expiryt ? e->expiryt :
				  tnow + settings->default_msgexpiry,
				  settings->optimize_notification_size);
	  octstr_destroy(transid);
	  octstr_destroy(url);
     } else if (mtype == MMS_MSGTYPE_DELIVERY_IND ||
		mtype == MMS_MSGTYPE_READ_ORIG_IND) 
	  smsg = msg;
     else {
	  error(0, "Unexpected message type %s for %s found in MT queue!", 
		mms_message_type_to_cstr(mtype), octstr_get_cstr(to));
	  res = MMS_SEND_ERROR_FATAL;
	  goto done;
     }
     
     if (smsg)
	  start_push(phonenum ? phonenum : rcpt_ip, 
		     phonenum ? 1 : 0, 
		     e, smsg); /* Send the message. 
				* Don't touch 'e' after this point!
				* It may be freed by receive thread. 
				*/
     
     if (smsg != msg && smsg)
	  mms_destroy(smsg);
    
 done:
     if (err != NULL && 
	 res != MMS_SEND_ERROR_TRANSIENT) { /* If there was a report request and this is a legit error
					     *  queue it. 
					     */
	  
	  if (dlr) {	       
	       MmsMsg *m = mms_deliveryreport(msgId, to, tnow, 
					      rtype ? octstr_imm(rtype) : 
					      octstr_imm("Indeterminate"));
	       
	       List *l = gwlist_create();
	       Octstr *res;
	       gwlist_append(l, from);
	       
	       /* Add to queue, switch via proxy to be from proxy. */
	       res = settings->qfs->mms_queue_add(to ? to : settings->system_user, l,  err, 
						  NULL, fromproxy,  
						  tnow, tnow+settings->default_msgexpiry, m, NULL, 
						  NULL, NULL,
						  NULL, NULL,
						  NULL,
						  0,
						  octstr_get_cstr(settings->mm1_queuedir), 
						  "MM2",
						  settings->host_alias);
	       gwlist_destroy(l, NULL);
	       mms_destroy(m);
	       octstr_destroy(res);
	  }
	  
     }
     
     /* Write to log */
     info(0, "%s Mobile Queue MMS Send Notify: From=%s, to=%s, msgsize=%d, reason=%s", 
	  SEND_ERROR_STR(res),
	  octstr_get_cstr(from), octstr_get_cstr(to), msize,
	  err ? octstr_get_cstr(err) : "");
     
     
     if (res == MMS_SEND_ERROR_FATAL) {
	  xto->process = 0;  /* No more attempts to deliver, delete this. */	       
	  if (settings->qfs->mms_queue_update(e) == 1) 
	       e = NULL; /* Queue entry gone. */	
	  else 	  
	       settings->qfs->mms_queue_free_env(e);
     }    /* Else queue will be updated/freed elsewhere. */
     
     
     if (prov_notify_event)
	  notify_prov_server(octstr_get_cstr(settings->prov_notify), 
			     to ? octstr_get_cstr(to) : "unknown", 
			     prov_notify_event, 
			     rtype ? rtype : "",
			     e ? e->msgId : NULL, NULL, NULL);

     if (msg) mms_destroy(msg);       

     octstr_destroy(phonenum);
     
     octstr_destroy(rcpt_ip);
     octstr_destroy(to);
     octstr_destroy(msgId);
     octstr_destroy(fromproxy);
     octstr_destroy(from);
     octstr_destroy(err);

     return 1;
}

void mbuni_mm1_queue_runner(int  *rstop)
{
     httpcaller = http_caller_create();
     if (gwthread_create((gwthread_func_t *)receive_push_reply, httpcaller) < 0) { /* Listener thread. */
	  error(0, "Mobile sender: Failed to create push reply thread: %d: %s!",
		errno, strerror(errno));
	  return;
     }

     settings->qfs->mms_queue_run(octstr_get_cstr(settings->mm1_queuedir), 
		   sendNotify, settings->queue_interval, settings->maxthreads, rstop);
     sleep(2); /* Wait for it to die. */
     http_caller_signal_shutdown(httpcaller);
     sleep(2);
     http_caller_destroy(httpcaller);
     return;     
}


