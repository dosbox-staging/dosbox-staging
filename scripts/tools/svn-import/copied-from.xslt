<?xml version="1.0" encoding="utf-8"?>
<!--
This file transforms svn log xml into path@rev representing copied-from information.
You can use it with xsltproc(1): $ svn log -xml -v -l 1 | copied-from.xslt -
-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="text"/>
<xsl:template match="/">
<xsl:value-of select="log/logentry/paths/path/@copyfrom-path"/>@<xsl:value-of select="log/logentry/paths/path/@copyfrom-rev"/>
</xsl:template>
</xsl:stylesheet>
