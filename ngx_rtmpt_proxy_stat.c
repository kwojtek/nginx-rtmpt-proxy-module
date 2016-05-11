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

static time_t                       rtmpt_proxy_start_time;

static char *ngx_rtmpt_proxy_stat(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_rtmpt_proxy_stat_postconfiguration(ngx_conf_t *cf);
static void * ngx_rtmpt_proxy_stat_create_loc_conf(ngx_conf_t *cf);
static char * ngx_rtmpt_proxy_stat_merge_loc_conf(ngx_conf_t *cf,
        void *parent, void *child);


typedef struct {
	ngx_uint_t						flag;
    ngx_str_t                       stylesheet;
} ngx_rtmpt_proxy_stat_loc_conf_t;

static ngx_conf_bitmask_t           ngx_rtmpt_proxy_stat_masks[] = {
    { ngx_string("on"),             1 },
    { ngx_null_string,              0 }
};

static ngx_command_t  ngx_rtmpt_proxy_stat_commands[] = {

    { ngx_string("rtmpt_proxy_stat"),
          NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
          ngx_rtmpt_proxy_stat,
          NGX_HTTP_LOC_CONF_OFFSET,
          offsetof(ngx_rtmpt_proxy_stat_loc_conf_t, flag),
          ngx_rtmpt_proxy_stat_masks },

    { ngx_string("rtmpt_proxy_stylesheet"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_rtmpt_proxy_stat_loc_conf_t, stylesheet),
        NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_rtmpt_proxy_stat_module_ctx = {
    NULL,                               /* preconfiguration */
    ngx_rtmpt_proxy_stat_postconfiguration,    /* postconfiguration */

    NULL,                               /* create main configuration */
    NULL,                               /* init main configuration */

    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */

    ngx_rtmpt_proxy_stat_create_loc_conf,      /* create location configuration */
    ngx_rtmpt_proxy_stat_merge_loc_conf,       /* merge location configuration */
};


ngx_module_t  ngx_rtmpt_proxy_stat_module = {
    NGX_MODULE_V1,
    &ngx_rtmpt_proxy_stat_module_ctx,          /* module context */
    ngx_rtmpt_proxy_stat_commands,             /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL, //ngx_rtmpt_proxy_stat_init_process,         /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};


static void
	ngx_rtmpt_proxy_stat_get(ngx_http_request_t *r)
{
	ngx_chain_t   				*out_chain;
	ngx_int_t					rc;
	ngx_buf_t    				*out_b;
	ngx_uint_t					os=0;
    ngx_rtmpt_proxy_stat_loc_conf_t  *plcf;

	
    plcf = ngx_http_get_module_loc_conf(r, ngx_rtmpt_proxy_stat_module);
 
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
		
	os = 100;
	char *ans=strdup("odpowiedz");
	os=strlen(ans);
	
	out_chain->next=NULL;
	out_chain->buf=out_b;
	
	out_b->memory = 1;
	out_b->last_buf = 1;
	out_b->pos = out_b->last = ans;
	
	out_b->last+=os;
	

	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "text/plain");
	r->headers_out.content_length_n = os;
	
	ngx_http_send_header(r);
	ngx_http_finalize_request(r, ngx_http_output_filter(r, out_chain) );
}


static ngx_int_t
	ngx_rtmpt_proxy_stat_handler(ngx_http_request_t *r) 
{
    ngx_rtmpt_proxy_stat_loc_conf_t  *plcf;

	
    plcf = ngx_http_get_module_loc_conf(r, ngx_rtmpt_proxy_stat_module);
    if (plcf->flag == 0) {
        return NGX_DECLINED;
    }
	
	//return NGX_DECLINED;
	int rc = ngx_http_read_client_request_body ( r , ngx_rtmpt_proxy_stat_get ) ;
  	if ( rc >= NGX_HTTP_SPECIAL_RESPONSE ) return rc;
	return NGX_DONE;
}



static void *
ngx_rtmpt_proxy_stat_create_loc_conf(ngx_conf_t *cf)
{
    ngx_rtmpt_proxy_stat_loc_conf_t       *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_rtmpt_proxy_stat_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->flag = 0;

    return conf;
}


static char *
ngx_rtmpt_proxy_stat_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_rtmpt_proxy_stat_loc_conf_t       *prev = parent;
    ngx_rtmpt_proxy_stat_loc_conf_t       *conf = child;

    ngx_conf_merge_bitmask_value(conf->flag, prev->flag, 0);
    ngx_conf_merge_str_value(conf->stylesheet, prev->stylesheet, "");

    return NGX_CONF_OK;
}

static char *
ngx_rtmpt_proxy_stat(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_rtmpt_proxy_stat_handler;

    return ngx_conf_set_bitmask_slot(cf, cmd, conf);
}

static ngx_int_t
ngx_rtmpt_proxy_stat_postconfiguration(ngx_conf_t *cf)
{
    rtmpt_proxy_start_time = ngx_cached_time->sec;

    return NGX_OK;
}

