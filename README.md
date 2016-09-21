# nginx-rtmpt-proxy-module

### Information
Simple nginx module with implementation of RTMPT protocol. RTMPT protocol is a HTTP tunnel for RTMP. It communicates over HTTP and passes data using HTTP POST requests. 

### Build

Download and unpack nginx source then cd to this directory. Next run command:

    ./configure --add-module=RTMPT_PROXY_MODULE_PATH
    make
    make install

### Configuration 

Basic configuration:

    http {
        server {
            listen       80;

            location ~ (^/open/1$|^/idle/.*/.*$|^/send/.*/.*$|^/close/.*/.*$) {
                rtmpt_proxy on;
                rtmpt_proxy_target TARGET-RTMP-SERVER.COM:1935;
                rtmpt_proxy_rtmp_timeout 2; 
                rtmpt_proxy_http_timeout 5;

                add_header Cache-Control no-cache;
                access_log off;
            }    
            location /fcs/ident2 {
                return 200;
            }
       }
    }

Directive 'location' with regexp fits all nessesery uri addresses for the rtmpt protocol. You can use other location for standard http requests.
Remember to add header "Cache-Control no-cache" to disable caching in user browser. Adding "access_log off" should help you keep clear access log.
 
TARGET-RTMP-SERVER.COM:1935 - connection url to rtmp server (for example ngxinx with nginx-rtmp-module).

rtmpt_proxy_rtmp_timeout - timeout in writing to rtmp server - in sec. default 2
rtmpt_proxy_http_time - timeout during waiting for http request - in sec. default 5

Additional location with '/fcs/ident2' is mandatory. For most usage it should return 200 without any content. 
Do not disable keep alive connection in configuration.

Statistic page configuration:

     location /stat {
       rtmpt_proxy_stat on;
       rtmpt_proxy_stylesheet stat.xsl;
     }
     location /stat.xsl {
       root /var/www/dirwithstat;
     }

In location /stat.xsl enter directory where file stat.xsl is located (you can copy this file from module path).
 

