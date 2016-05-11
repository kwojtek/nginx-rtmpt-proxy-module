# nginx-rtmpt-proxy-module

### Information
Simple nginx module with implementation of RTMPT protocol. RTMPT protocol is a HTTP tunnel for RTMP. It communicates over HTTP and passes data using HTTP POST requests. 

ATTENTION! it is alpha state. Do not use on production :).

### Build

Download and unpack nginx source then cd to this directory. Next run command:

    ./configure --add-module=RTMPT_PROXY_MODULE_PATH
    make
    make install

### Configuration 
http {
    server {
        listen       EXTERNALIP:80;

	location ~ (^/fcs/ident2$|^/open/1$|^/idle/.*/.*$|^/send/.*/.*$|^/close/$) {
            rtmpt_proxy on;
            rtmpt_proxy_target TARGET-RTMP-SERVER.COM:1935;
            rtmpt_proxy_ident EXTERNALIP;
            add_header Cache-Control no-cache;
            access_log off;
        }    
    }
}

Directive 'location' fits all nessesery uri addresses for the rtmpt protocol. You can use other location for standard http requests.
It is good practice to add header "Cache-Control no-cache" to disable caching in user browser and for me disabling access_log could keep clear your access log.
 
TARGET-RTMP-SERVER.COM:1935 - connection url to rtmp server (for example ngxinx with nginx-rtmp-module).
EXTERNALIP - your external IP interface. Remeber to set it in "rtmpt_proxy_ident".

### TODO
* timeout in config
* stat page
* check log information on connect/disconnect and on errors
* eliminate rtmpt_proxy_ident - send 404 instend of 200 - but now it doesn't work
* add checking proper sequence in idle and send requests
* ....

