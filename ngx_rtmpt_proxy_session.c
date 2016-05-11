/* 
 * Copyright clickmeeting.com 
 * Wojtek Kosak <wkosak@gmail.com>
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include "ngx_rtmpt_proxy_session.h"
#include "ngx_rtmpt_proxy_module.h"
#include "ngx_rtmpt_send.h"

#define RTMPT_HASHMAX 1000

ngx_rtmpt_proxy_session_t  *ngx_rtmpt_proxy_sessions_global[RTMPT_HASHMAX];



static ngx_int_t
ngx_rtmpt_proxy_session_get_peer(ngx_peer_connection_t *pc, void *data)
{
    return NGX_OK;
}

    
static void
ngx_rtmpt_proxy_session_free_peer(ngx_peer_connection_t *pc, void *data,
            ngx_uint_t state)
{
}

static void session_name_create(char *buf, int size) {
   int i;
   for (i=0;i<size;i++) {
     buf[i]=(rand()%2?((rand()%2?'a':'A')+rand()%26):'0'+rand()%10);
   }
}


static ngx_rtmpt_proxy_session_t *get_session_from_hash(char *name, int size) {

   ngx_rtmpt_proxy_session_t    **sessions, *ts; 
   
   int hash=ngx_hash_key(name,size)%RTMPT_HASHMAX;

   sessions = ngx_rtmpt_proxy_sessions_global; 
   
  
   for (ts=sessions[hash];ts;ts=ts->next) {
     
     if (ts->name.len==size && strncmp(ts->name.data,name,size)==0) {
        return ts;
     }
   }
   return NULL;
}

static void put_session_in_hash(ngx_rtmpt_proxy_session_t *session) {

   ngx_rtmpt_proxy_session_t    **sessions, *ts=NULL, *cs=NULL;
   
   int hash=ngx_hash_key(session->name.data,session->name.len)%RTMPT_HASHMAX;
   
   sessions = ngx_rtmpt_proxy_sessions_global; 
	
   if (!sessions[hash]) {
      sessions[hash]=session;
      return;
   }
   for (ts=sessions[hash];ts;ts=ts->next) {
     cs=ts;
   }
   
   cs->next=session;
   session->prev=cs;
}

static void remove_session_from_hash(char *name, int size) {
    ngx_rtmpt_proxy_session_t    **sessions, *ts=NULL;
	
    int hash=ngx_hash_key(name,size)%RTMPT_HASHMAX;
	
	sessions = ngx_rtmpt_proxy_sessions_global; 
	
    for (ts=sessions[hash];ts;ts=ts->next) {
     if (ts->name.len==size && strncmp(ts->name.data,name,size)==0) {
        
        if (ts->next) {
           ts->next->prev=ts->prev;
        }
        if (ts->prev) {
           ts->prev->next=ts->next;
        }
        if (ts == sessions[hash]) {
           sessions[hash]=ts->next;
        }
        return;
     }
   }
   return;
}



ngx_rtmpt_proxy_session_t 
	*ngx_rtmpt_proxy_create_session(ngx_http_request_t *r) 
{
	ngx_rtmpt_proxy_session_t 		*session;
	ngx_peer_connection_t     		*pc;
	ngx_rtmpt_proxy_loc_conf_t  	*plcf;
	ngx_pool_t                     	*pool;
	ngx_url_t                   	url;
	int 							rc;
	
	
	plcf = ngx_http_get_module_loc_conf(r, ngx_rtmpt_proxy_module);
	
	if (!plcf) {
		goto error;
	}
	
	
	pool = ngx_create_pool(4096, plcf->log);
	if (pool == NULL) {
		ngx_log_error(NGX_LOG_ERR, plcf->log, 0, "rtmpt/session: cannot create pool for session");
		goto error;
	}
	
	
	session = (ngx_rtmpt_proxy_session_t *) ngx_pcalloc(pool, sizeof(ngx_rtmpt_proxy_session_t));
	if (session == NULL) {
		ngx_log_error(NGX_LOG_ERR, plcf->log, 0, "rtmpt/session: cannot allocate memory for session");
		goto error;
	}
	
	session->name.data=NULL;
	session->name.len=0;
	
	ngx_str_set(&session->name, "1234567890123456");
	session->name.data = ngx_pstrdup(pool, &session->name);
	session_name_create(session->name.data,session->name.len);
		
	session->log = plcf->log;
	session->pool = pool;
	session->sequence = 0;   
	session->from_rtmp = NULL;
	session->to_rtmp = NULL;
	session->on_finish_send = NULL;
	session->chain_from_http_request = NULL;   
	session->chain_from_nginx = NULL;
	session->out_pool = NULL;
	session->interval_check_time=0;
	session->interval_check_att=0;
	
	put_session_in_hash(session);
	
	pc = ngx_pcalloc(pool, sizeof(ngx_peer_connection_t));
	if (pc == NULL) {
		ngx_log_error(NGX_LOG_ERR, plcf->log, 0, "rtmpt/session: cannot allocate for peer connection");
		goto error;
	} 
	
	ngx_memzero(&url, sizeof(ngx_url_t));
	url.url.data = plcf->target.data;
	url.url.len = plcf->target.len;
	url.default_port = 1935;
	url.uri_part = 1;
	
	if (ngx_parse_url(pool, &url) != NGX_OK) {
		ngx_log_error(NGX_LOG_ERR, plcf->log, 0, "rtmpt/session: error [%s] failed to parse server name: %V", url.err, &url.url);
		goto error;
	}
	
	
	ngx_memzero(pc, sizeof(ngx_peer_connection_t));
	pc->log = session->log;
    pc->get = ngx_rtmpt_proxy_session_get_peer;
    pc->free = ngx_rtmpt_proxy_session_free_peer;
	
	pc->sockaddr = url.addrs[0].sockaddr;
	pc->socklen = url.addrs[0].socklen;
	pc->name = &url.addrs[0].name;
    

    rc = ngx_event_connect_peer(pc);
	if (rc != NGX_OK && rc != NGX_AGAIN ) {
		ngx_log_error(NGX_LOG_ERR, plcf->log, 0, "rtmpt/session: error in connect peer");
		goto error;
	}

	pc->connection->data = session;
	pc->connection->read->handler = ngx_rtmpt_read_from_rtmp;
	pc->connection->write->handler = ngx_rtmpt_send_chain_to_rtmp; 
	pc->connection->idle = 0;
	pc->connection->log = session->log;
	pc->connection->pool = session->pool;
	pc->connection->pool->log = session->log;
	pc->connection->read->log = session->log;
	pc->connection->write->log = session->log;
	
	
	
	
	session->connection = pc->connection;
	
   	return session;
	
error:
	if (pool) {
		ngx_destroy_pool(pool);
	}
	return NULL;
}


ngx_rtmpt_proxy_session_t 
	*ngx_rtmpt_proxy_get_session(ngx_str_t *id) 
{
	return get_session_from_hash(id->data,id->len);
}


void 
	ngx_rtmpt_proxy_destroy_session(ngx_rtmpt_proxy_session_t *session) 
{
	remove_session_from_hash(session->name.data, session->name.len);
	ngx_close_connection(session->connection);
	ngx_destroy_pool(session->pool);
	if (session->out_pool) {
		ngx_destroy_pool(session->out_pool);
		session->out_pool = NULL;
	}
}


