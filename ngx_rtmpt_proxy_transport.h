/* 
 * Copyright clickmeeting.com 
 * Wojtek Kosak <wkosak@gmail.com>
 */

#ifndef _NGX_RTMPT_PROXY_TRANSPORT_H_INCLUDED_
#define _NGX_RTMPT_PROXY_TRANSPORT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

void ngx_rtmpt_send_chain_to_rtmp(ngx_event_t *ev);
void ngx_rtmpt_read_from_rtmp(ngx_event_t *ev);

#endif
