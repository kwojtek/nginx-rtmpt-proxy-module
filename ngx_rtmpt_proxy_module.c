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

static char *ngx_rtmpt_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_rtmpt_proxy_create_loc_conf(ngx_conf_t *cf);
static char *ngx_rtmpt_proxy_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
void ngx_rtmpt_finish_proxy_send(ngx_rtmpt_proxy_session_t *s);
static ngx_int_t ngx_rtmpt_proxy_handler(ngx_http_request_t *r);

static void ngx_rtmpt_proxy_close(ngx_http_request_t *r);
static void ngx_rtmpt_finish_proxy_process(ngx_rtmpt_proxy_session_t *s);

static ngx_conf_bitmask_t           ngx_rtmpt_proxy_masks[] = {
    { ngx_string("on"),             1 },
    { ngx_null_string,              0 }
};

static ngx_command_t  ngx_rtmpt_proxy_module_commands[] = {
  { ngx_string("rtmpt_proxy_target"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_rtmpt_proxy_loc_conf_t, target),
        NULL },
  { ngx_string("rtmpt_proxy_ident"),
		NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_rtmpt_proxy_loc_conf_t, ident),
		NULL },
  { ngx_string("rtmpt_proxy"),
        NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
        ngx_rtmpt_proxy,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_rtmpt_proxy_loc_conf_t, proxy_flag),
        ngx_rtmpt_proxy_masks },

  ngx_null_command
};

static ngx_http_module_t  ngx_rtmpt_proxy_module_ctx = {
    NULL,                               /* preconfiguration */
    NULL,    				/* postconfiguration */

    NULL,                               /* create main configuration */
    NULL,                               /* init main configuration */

    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */

    ngx_rtmpt_proxy_create_loc_conf,	/* create location configuration */
    ngx_rtmpt_proxy_merge_loc_conf,     /* merge location configuration */
};

ngx_module_t  ngx_rtmpt_proxy_module = {
    NGX_MODULE_V1,
    &ngx_rtmpt_proxy_module_ctx,         /* module context */
    ngx_rtmpt_proxy_module_commands,    /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,         						/* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};


static void idle_session_checker(ngx_event_t *ev) {
	ngx_rtmpt_proxy_session_t	*s;
	
	s=ev->data;
	ngx_log_error(NGX_LOG_INFO, ev->log, 0, "Timeout during waiting for http request id=%V",&s->name);
	ngx_rtmpt_proxy_destroy_session(s);
}

static void
	ngx_rtmpt_proxy_open(ngx_http_request_t *r)
{
	ngx_buf_t    				*b;
	ngx_chain_t   				out;
	ngx_rtmpt_proxy_session_t	*s;
	ngx_int_t					rc;
	
	
	
	
	b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	
	if (!b) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
		"Failed to allocate response buffer.");
		
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	
	s = ngx_rtmpt_proxy_create_session(r);
	if (!s) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
		"Failed in getting session");
		
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
		
	b->pos = b->last = ngx_pcalloc(r->pool, s->name.len+10);
	ngx_sprintf(b->pos, "%V\n",&s->name);
	b->last+=(s->name.len+1);


	s->http_timer.handler = idle_session_checker;
	s->http_timer.log = s->log;
	s->http_timer.data = s;
	ngx_add_timer(&s->http_timer, 2000);
	
	
	b->memory = 1;
	b->last_buf = 1;
	
	out.buf = b;
	out.next = NULL;

	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "application/x-fcs");
	r->headers_out.content_length_n = b->last-b->pos;
	ngx_http_send_header(r);

	ngx_http_finalize_request(r, ngx_http_output_filter(r, &out) );
}


static void
	ngx_rtmpt_proxy_close(ngx_http_request_t *r)
{
	ngx_chain_t   				*out_chain;
	ngx_rtmpt_proxy_session_t	*s;
	ngx_int_t					rc;
	ngx_buf_t    				*out_b;
	u_char						*buffer;
	ngx_uint_t					os=0;
	ngx_http_cleanup_t 			*cuh;
	
	
	s = ngx_rtmpt_proxy_create_session(r);
	if (!s) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
		"Failed in getting session");
		
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	
	
	out_chain = ngx_alloc_chain_link(r->pool);
	if (!out_chain) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"Failed to allocate response chain");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	}
	out_b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	
	if (!out_b) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	}
	
	buffer = ngx_pcalloc(r->pool, 1);
	if (!buffer) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response one byte's buffer");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	}
	*buffer = 0;
	os = 1;
	
	out_chain->next=NULL;
	out_chain->buf=out_b;
	
	out_b->memory = 1;
	out_b->last_buf = 1;
	out_b->pos = out_b->last = buffer;
	
	out_b->last+=1;
	

	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "application/x-fcs");
	r->headers_out.content_length_n = os;
	
	
	cuh=ngx_http_cleanup_add(r,0);
	cuh->handler=ngx_rtmpt_proxy_destroy_session; 
	cuh->data=s;
	
	ngx_http_send_header(r);
	ngx_http_finalize_request(r, ngx_http_output_filter(r, out_chain) );
}


static void
	ngx_rtmpt_proxy_ident(ngx_http_request_t *r)
{
	ngx_chain_t   				*out_chain;
	ngx_int_t					rc;
	ngx_buf_t    				*out_b;
	ngx_uint_t					os=0;
	ngx_http_cleanup_t 			*cuh;
    ngx_rtmpt_proxy_loc_conf_t  *plcf;

	
    plcf = ngx_http_get_module_loc_conf(r, ngx_rtmpt_proxy_module);
 
	out_chain = ngx_alloc_chain_link(r->pool);
	if (!out_chain) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"Failed to allocate response chain");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	}
	out_b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	
	if (!out_b) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	}
	

	
	os = plcf->ident.len;
	
	out_chain->next=NULL;
	out_chain->buf=out_b;
	
	out_b->memory = 1;
	out_b->last_buf = 1;
	out_b->pos = out_b->last = plcf->ident.data;
	
	out_b->last+=os;
	

	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "text/plain");
	r->headers_out.content_length_n = os;
	
	ngx_http_send_header(r);
	ngx_http_finalize_request(r, ngx_http_output_filter(r, out_chain) );
}


static void 
	ngx_rtmpt_proxy_process(ngx_http_request_t *r)
{
	ngx_buf_t    				*out_b;
	ngx_chain_t   				out;
	ngx_rtmpt_proxy_session_t	*s;
	ngx_chain_t 				*cl, *in = r->request_body->bufs;
	int							outsize;
	ngx_buf_t 					*b;
	char						*outbuf;
	ngx_uint_t					bytes_received=0;
	
	char						*idpos=NULL;
	int							idlen=0;
	
	ngx_str_t					sessionid;	
	
	
	sessionid.data=NULL;
	sessionid.len=0;
	
	if (*(r->uri.data+5)=='/') {
		char *c;
		
		sessionid.data=r->uri.data+6;
		for (sessionid.len=0,c=sessionid.data;*c!='/' || sessionid.len+6==r->uri.len;sessionid.len++,c++);
		if (*c!='/') sessionid.data=NULL;
	}
	//TODO: check sequence ofter URI
	
	
	if (!sessionid.data) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"Cannot find session ID for URI=%V", &r->uri);
		ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
		return;
	}
	
	s = ngx_rtmpt_proxy_get_session(&sessionid);
	
	if (!s) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"Cannot find session for URI=%V", &r->uri);
		ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
		return;
	}
	
	ngx_add_timer(&s->http_timer, 2000);
		
	if (strncasecmp("/send/",r->uri.data,6) == 0) {
		for (cl = in; cl; cl=cl->next) {
			b = cl->buf;
			bytes_received+=b->last - b->pos;
		}
	} 
	 
	s->on_finish_send = ngx_rtmpt_finish_proxy_process;
	s->actual_request = r;
	
	if (bytes_received>0) {
		s->chain_from_http_request = in; 
		if (in && in->buf) s->buf_pos = in->buf->pos;
	} else {
		s->chain_from_http_request = NULL;
		s->buf_pos = NULL;
	}
	
	ngx_rtmpt_send_chain_to_rtmp(s->connection->write);
}


static void ngx_rtmpt_finish_proxy_process(ngx_rtmpt_proxy_session_t *s) {	
	
	ngx_chain_t   				*out_chain;
	ngx_http_request_t 			*r;
	ngx_uint_t					os = 0;
	time_t	check_time;

	r=s->actual_request;
	s->on_finish_send=NULL;	
	
	
	if (s->chain_from_nginx) {		
		ngx_chain_t *t;
		ngx_http_cleanup_t *cuh;
	
		*s->chain_from_nginx->buf->pos=ngx_rtmpt_proxy_intervals_def[s->interval_position];
		
		
		for (t=s->chain_from_nginx; t; t=t->next) {
			os+=t->buf->last-t->buf->pos;
			if (!t->next) t->buf->last_buf=1; else t->buf->last_buf=0;
		}
		out_chain = s->chain_from_nginx;
		s->chain_from_nginx=NULL;
		
	
		cuh=ngx_http_cleanup_add(r,0);
		cuh->handler=ngx_destroy_pool; 
		cuh->data=s->out_pool;
 	   
		s->out_pool=NULL;
	} else {
		ngx_buf_t    	*out_b;
		u_char			*buffer;
		
		out_chain = ngx_alloc_chain_link(r->pool);
		if (!out_chain) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"Failed to allocate response chain");
			goto error;
		}
		out_b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
		
		if (!out_b) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer");
			goto error;
		}
		
		buffer = ngx_pcalloc(r->pool, 1);
		if (!buffer) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response one byte's buffer");
			goto error;
		}
		*buffer = ngx_rtmpt_proxy_intervals_def[s->interval_position];
		os = 1;
		
		out_chain->next=NULL;
		out_chain->buf=out_b;
		
		out_b->memory = 1;
		out_b->last_buf = 1;
		out_b->pos = out_b->last = buffer;
		
		out_b->last+=1;
	}
	
	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "application/x-fcs");
	r->headers_out.content_length_n = os;
	
	
	
	time(&check_time);
	if (s->interval_check_time != check_time) {
		double d;
		
		if (s->interval_check_count) {
			d=(double)s->interval_check_att/s->interval_check_count;
			if (d > 0.4) {
				if (ngx_rtmpt_proxy_intervals_def[s->interval_position+1]!=0) 
					s->interval_position++;
			} else if (d == 0) {
				if (ngx_rtmpt_proxy_intervals_def[s->interval_position-1]!=0) 
					s->interval_position--;
			}
		}
				
		s->interval_check_att = 0;
		s->interval_check_count = 0;
		s->interval_check_time=check_time;
	}
	
	//if no data
	if ( os == 1 ) s->interval_check_att++;
	s->interval_check_count++;
	
	
	ngx_http_send_header(r);
	ngx_http_finalize_request(r, ngx_http_output_filter(r, out_chain));
	return;
	
error:
	ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
	
}

static ngx_int_t
	ngx_rtmpt_proxy_handler(ngx_http_request_t *r) 
{
    ngx_rtmpt_proxy_loc_conf_t  *plcf;

	
    plcf = ngx_http_get_module_loc_conf(r, ngx_rtmpt_proxy_module);
    if (plcf->proxy_flag == 0) {
        return NGX_DECLINED;
    }

	if (strncasecmp("/fcs/ident2",r->uri.data,11) == 0) {
		int rc = ngx_http_read_client_request_body ( r , ngx_rtmpt_proxy_ident ) ;
  	  	if ( rc >= NGX_HTTP_SPECIAL_RESPONSE ) return rc;
		return NGX_DONE;
	}

	if (strncasecmp("/open/1",r->uri.data,7) == 0) {
		int rc = ngx_http_read_client_request_body ( r , ngx_rtmpt_proxy_open ) ;
  	  	if ( rc >= NGX_HTTP_SPECIAL_RESPONSE ) return rc;
		return NGX_DONE;
	}
    
	if (strncasecmp("/idle/",r->uri.data,6) == 0 || strncasecmp("/send/",r->uri.data,6) == 0) {
		int rc = ngx_http_read_client_request_body ( r , ngx_rtmpt_proxy_process ) ;
  	  	if ( rc >= NGX_HTTP_SPECIAL_RESPONSE ) return rc;
		return NGX_DONE;
	}
	if (strncasecmp("/close/",r->uri.data,7)==0) {
		int rc = ngx_http_read_client_request_body ( r , ngx_rtmpt_proxy_close ) ;
  	  	if ( rc >= NGX_HTTP_SPECIAL_RESPONSE ) return rc;
		return NGX_DONE;
	}
	
	return NGX_DECLINED;
}


static char *
	ngx_rtmpt_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;


	ngx_log_debug0(NGX_LOG_DEBUG, cf->log, 0, "enter ngx_rtmpt_proxy enabled");

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_rtmpt_proxy_handler;

    return ngx_conf_set_bitmask_slot(cf, cmd, conf);
}

static void *
	ngx_rtmpt_proxy_create_loc_conf(ngx_conf_t *cf)
{
    ngx_rtmpt_proxy_loc_conf_t       *conf;

	ngx_log_debug0(NGX_LOG_DEBUG, cf->log, 0, "enter ngx_rtmpt_proxy create");

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_rtmpt_proxy_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

	conf->log = &cf->cycle->new_log;
    conf->proxy_flag = 0;

    return conf;
}


static char *
	ngx_rtmpt_proxy_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_rtmpt_proxy_loc_conf_t       *prev = parent;
    ngx_rtmpt_proxy_loc_conf_t       *conf = child;

    ngx_conf_merge_bitmask_value(conf->proxy_flag, prev->proxy_flag, 0);
    ngx_conf_merge_str_value(conf->target, prev->target, "localhost:1935");
    ngx_conf_merge_str_value(conf->ident, prev->ident, "127.0.0.1");
 
    return NGX_CONF_OK;
}


