<?xml version="1.0" encoding="utf-8" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
    <html>
        <head>
            <title>RTMPT proxy stats</title>
        </head>
        <body>
	    <h1>RTMPT proxy stats</h1>
            <xsl:apply-templates select="//rtmpt/sessions"/>
            <hr width="100%" />
 	    <table><tr><td><b>uptime:</b></td><td><xsl:call-template name="showtime"><xsl:with-param name="time" select="//rtmpt/uptime * 1000"/></xsl:call-template></td>
            <td><b>created sessions:</b></td><td><xsl:value-of select="rtmpt/sessions_created"/></td>
            <td><b>bytes in:</b></td><td><xsl:call-template name="showsize"><xsl:with-param name="size" select="rtmpt/bytes_from_http"/></xsl:call-template></td>
            <td><b>bytes out:</b></td><td><xsl:call-template name="showsize"><xsl:with-param name="size" select="rtmpt/bytes_to_http"/></xsl:call-template></td>
            </tr></table>
            <table><tr>
            <td ><b>nginx ver.:</b></td><td><xsl:value-of select="rtmpt/ngx_version"/></td>
            <td ><b>rtmpt mod. ver.:</b></td><td><xsl:value-of select="rtmpt/ngx_rtmpt_proxy_version" /></td>
            </tr></table> 

        </body>
    </html>
</xsl:template>

<xsl:template match="//rtmpt/sessions">
<table cellspacing="1" cellpadding="5" width="100%">
<tr bgcolor="#a1a1a1"><th>num</th><th>id</th><th>live time</th><th>create ip</th><th>target url</th><th>req. count</th><th>bytes in</th><th>bytes out</th></tr>
  <xsl:apply-templates select="session"/>
</table>
</xsl:template>

<xsl:template match="session">
<xsl:variable name="bgcolor">
        <xsl:choose>
            <xsl:when test="position() mod 2 = 1">#FFFFFF</xsl:when>
            <xsl:otherwise>#c0c0c0</xsl:otherwise>
        </xsl:choose>
    </xsl:variable>
   <tr bgcolor="{$bgcolor}">
  <td><xsl:number value="position()" format="1" /></td>
  <td><xsl:value-of select="id" /></td>
  <td><xsl:call-template name="showtime">
               <xsl:with-param name="time" select="uptime * 1000"/>
            </xsl:call-template></td>
  <td><xsl:value-of select="create_ip" /></td>
  <td><xsl:value-of select="target_url" /></td>
  <td><xsl:value-of select="requests_count" /></td>
  <td><xsl:call-template name="showsize"><xsl:with-param name="size" select="bytes_from_http"/></xsl:call-template></td>
  <td><xsl:call-template name="showsize"><xsl:with-param name="size" select="bytes_to_http"/></xsl:call-template></td>
</tr>
</xsl:template>

<xsl:template name="showtime">
    <xsl:param name="time"/>

    <xsl:if test="$time &gt; 0">
        <xsl:variable name="sec">
            <xsl:value-of select="floor($time div 1000)"/>
        </xsl:variable>

        <xsl:if test="$sec &gt;= 86400">
            <xsl:value-of select="floor($sec div 86400)"/>d
        </xsl:if>

        <xsl:if test="$sec &gt;= 3600">
            <xsl:value-of select="(floor($sec div 3600)) mod 24"/>h
        </xsl:if>

        <xsl:if test="$sec &gt;= 60">
            <xsl:value-of select="(floor($sec div 60)) mod 60"/>m
        </xsl:if>

        <xsl:value-of select="$sec mod 60"/>s
    </xsl:if>
</xsl:template>

<xsl:template name="showsize">
    <xsl:param name="size"/>
    <xsl:variable name="sizen">
        <xsl:value-of select="floor($size div 1024)"/>
    </xsl:variable>
    <xsl:choose>
        <xsl:when test="$sizen &gt;= 1073741824">
            <xsl:value-of select="format-number($sizen div 1073741824,'#.###')"/>TB</xsl:when>

        <xsl:when test="$sizen &gt;= 1048576">
            <xsl:value-of select="format-number($sizen div 1048576,'#.###')"/>GB</xsl:when>

        <xsl:when test="$sizen &gt;= 1024">
            <xsl:value-of select="format-number($sizen div 1024,'#.##')"/>MB</xsl:when>
        <xsl:when test="$sizen &gt;= 0">
            <xsl:value-of select="$sizen"/>kB</xsl:when>
    </xsl:choose>
</xsl:template>

</xsl:stylesheet>
