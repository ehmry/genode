<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" indent="yes"/>
  <xsl:template match = "/">
    <config>

      <parent-provides>
	<xsl:for-each select="expr/attrs/attr[@name='parent-provides']/list/string">
          <service name="{@value}"/>
	</xsl:for-each>
      </parent-provides>

      <default-route>
	<any-service> <parent/> <any-child/> </any-service>
      </default-route>

      <xsl:for-each select="expr/attrs/attr[@name='children']/list/attrs">
	<start name="{attr[@name='name']/string/@value}">

        <xsl:for-each select="attr[@name='resources']/attrs">
          <resource>
            <xsl:for-each select="attr">

              <xsl:attribute name="name">
                <xsl:value-of select="@name"/>
              </xsl:attribute>
              <xsl:attribute name="quantum">
                <xsl:value-of select="string/@value"/>
              </xsl:attribute>

            </xsl:for-each>
          </resource>
        </xsl:for-each>

	<xsl:if test="attr[@name='provides']/list/*">
          <provides>
            <xsl:for-each select="attr[@name='provides']/list">
              <service>
		<xsl:attribute name="name">
                  <xsl:value-of select="string/@value"/>
		</xsl:attribute>
              </service>
            </xsl:for-each>
          </provides>
	</xsl:if>

      </start>
    </xsl:for-each>

  </config>
  </xsl:template>
</xsl:stylesheet>
