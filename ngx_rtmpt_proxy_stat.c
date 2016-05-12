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
#include "ngx_rtmpt_proxy_transport.h"
#include "ngx_rtmpt_proxy_version.h"
 
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
ngx_rtmpt_proxy_stat_output(ngx_http_request_t *r, ngx_chain_t ***lll, void *data, size_t len) {
	ngx_chain_t *c;
	ngx_buf_t   *b;
	
	
	if (!len)
		return;

	c = **lll;
	if (c && c->buf->last + len > c->buf->end) {
	    *lll = &c->next;
	}
	
	if (**lll == NULL) {
		c = ngx_alloc_chain_link(r->pool);
		if (c == NULL) {
			ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
			return;
		}
		b = ngx_create_temp_buf(r->pool, ngx_max(8192, len));
		
		if (b == NULL || b->pos == NULL) {
			ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
			return;
		}
		c->next = NULL;
		c->buf = b;
		**lll = c;
	};	
	
	b = (**lll)->buf;
	
	b->last = ngx_cpymem(b->last, data, len);
}

#define NGX_RTMPT_PROXY_STAT(data, len)    ngx_rtmpt_proxy_stat_output(r, lll, data, len)
#define NGX_RTMPT_PROXY_STAT_L(s)	NGX_RTMPT_PROXY_STAT((s), sizeof(s) - 1)


static void
	ngx_rtmpt_proxy_stat_get(ngx_http_request_t *r)
{
	ngx_uint_t					os=0;
    ngx_rtmpt_proxy_stat_loc_conf_t  *plcf;
	ngx_chain_t                    *cl, *l, **ll, ***lll;
	static u_char                   buf[1024];
	ngx_rtmpt_proxy_session_t		**sessions,*s;
	ngx_uint_t						sessions_hs,i;


	
    plcf = ngx_http_get_module_loc_conf(r, ngx_rtmpt_proxy_stat_module);
	if (!plcf) {
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	cl = NULL;
	ll = &cl;
	lll = &ll;
	
	NGX_RTMPT_PROXY_STAT_L("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n");
	if (plcf->stylesheet.len) {
		NGX_RTMPT_PROXY_STAT_L("<?xml-stylesheet type=\"text/xsl\" href=\"");
		NGX_RTMPT_PROXY_STAT(plcf->stylesheet.data, plcf->stylesheet.len);
		NGX_RTMPT_PROXY_STAT_L("\" ?>\r\n");
	}
	NGX_RTMPT_PROXY_STAT_L("<rtmpt>\r\n");
	NGX_RTMPT_PROXY_STAT_L("<ngx_version>" NGINX_VERSION "</ngx_version>\r\n");
	NGX_RTMPT_PROXY_STAT_L("<ngx_rtmpt_proxy_version>" NGINX_RTMPT_PROXY_VERSION "</ngx_rtmpt_proxy_version>\r\n");
	NGX_RTMPT_PROXY_STAT_L("<uptime>");
	NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%T", ngx_cached_time->sec - rtmpt_proxy_start_time) - buf);
	NGX_RTMPT_PROXY_STAT_L("</uptime>\r\n");
	NGX_RTMPT_PROXY_STAT_L("<sessions_created>");
	NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%ui",ngx_rtmpt_proxy_sessions_created) - buf);
	NGX_RTMPT_PROXY_STAT_L("</sessions_created>\r\n");	
	NGX_RTMPT_PROXY_STAT_L("<bytes_from_http>");
	NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%ui",ngx_rtmpt_proxy_bytes_from_http) - buf);	
	NGX_RTMPT_PROXY_STAT_L("</bytes_from_http>");
	NGX_RTMPT_PROXY_STAT_L("<bytes_to_http>");
	NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%ui",ngx_rtmpt_proxy_bytes_to_http) - buf);
	NGX_RTMPT_PROXY_STAT_L("</bytes_to_http>");
	
	sessions=ngx_rtmpt_proxy_session_getall(&sessions_hs);
	NGX_RTMPT_PROXY_STAT_L("<sessions>\r\n");
	for (i=0;i<sessions_hs;i++) {
		for (s=sessions[i];s;s=s->next) {
			NGX_RTMPT_PROXY_STAT_L("<session>");
			NGX_RTMPT_PROXY_STAT_L("<id>");
			NGX_RTMPT_PROXY_STAT(s->name.data, s->name.len);
			NGX_RTMPT_PROXY_STAT_L("</id>");
			NGX_RTMPT_PROXY_STAT_L("<uptime>");
			NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%T", ngx_cached_time->sec - s->created_at) - buf);
			NGX_RTMPT_PROXY_STAT_L("</uptime>");
			NGX_RTMPT_PROXY_STAT_L("<create_ip>");
			NGX_RTMPT_PROXY_STAT(s->create_request_ip.data,s->create_request_ip.len);
			NGX_RTMPT_PROXY_STAT_L("</create_ip>");
			NGX_RTMPT_PROXY_STAT_L("<target_url>");
			NGX_RTMPT_PROXY_STAT(s->target_url.data,s->target_url.len);
			NGX_RTMPT_PROXY_STAT_L("</target_url>");
			NGX_RTMPT_PROXY_STAT_L("<requests_count>");
			NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%ui",s->http_requests_count) - buf);
			NGX_RTMPT_PROXY_STAT_L("</requests_count>");
			NGX_RTMPT_PROXY_STAT_L("<bytes_from_http>");
			NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%ui",s->bytes_from_http) - buf);
			NGX_RTMPT_PROXY_STAT_L("</bytes_from_http>");
			NGX_RTMPT_PROXY_STAT_L("<bytes_to_http>");
			NGX_RTMPT_PROXY_STAT(buf, ngx_sprintf(buf, "%ui",s->bytes_to_http) - buf);
			NGX_RTMPT_PROXY_STAT_L("</bytes_to_http>");
			NGX_RTMPT_PROXY_STAT_L("</session>\r\n");
		}
	}
	NGX_RTMPT_PROXY_STAT_L("</sessions>\r\n");
	
	NGX_RTMPT_PROXY_STAT_L("</rtmpt>\r\n");
		
		
	os = 0;
	for (l = cl; l; l = l->next) {
	    os += (l->buf->last - l->buf->pos);
	}
	
	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "text/xml");
	r->headers_out.content_length_n = os;
	
	(*ll)->buf->last_buf = 1;
	ngx_http_send_header(r);
	
	ngx_http_finalize_request(r, ngx_http_output_filter(r, cl) );
	
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

