<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" indent="yes"/>
  <xsl:template match = "/">

  <config verbose="yes" >

    <parent-provides>
      <xsl:for-each select="expr/attrs/attr[@name='parent-provides']/list/string">
        <service name="{@value}"/>
      </xsl:for-each>
    </parent-provides>

    <default-route>
      <any-service> <parent/> </any-service>
    </default-route>

    <!--
    <xsl:for-each select="expr/attrs/attr[@name='defaultRoute']/list">
      <default-route>

        <xsl:for-each select="list">
          <xsl:choose>

            <xsl:when test="string[1]/@value='any-service'">
              <any-service>
                <xsl:for-each select="string[position()>1]">
                  <xsl:if test="./@value='parent'"><parent/></xsl:if>
                  <xsl:if test="./@value='any-child'"><any-child/></xsl:if>
                </xsl:for-each>
              </any-service>
            </xsl:when>

            missing logic to deal with other routes

          </xsl:choose>
        </xsl:for-each>

      </default-route>
    </xsl:for-each>
    -->

    <xsl:for-each select="expr/attrs/attr[@name='children']/list/attrs">
      <start name="{attr[@name='name']/string/@value}">

        <!--
        <xsl:for-each select="attrs/attr[@name='binary']">
          <!- include <binary/> if the name and binary are different ->
          <xsl:if test="string/@value!=../../@name">
            <binary name="{string/@value}"/>
          </xsl:if>
          </xsl:for-each>
        -->

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

        <provides>
          <xsl:for-each select="attr[@name='provides']/list">

            <service>
              <xsl:attribute name="name">
                <xsl:value-of select="string/@value"/>
              </xsl:attribute>
            </service>
          </xsl:for-each>
        </provides>

      </start>
    </xsl:for-each>

  </config>

  </xsl:template>

</xsl:stylesheet>
