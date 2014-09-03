<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" indent="yes"/>
  <xsl:template match = "/">

  <config>

    <parent-provides>
      <xsl:for-each select="expr/attrs/attr[@name='parentProvides']/list/string">
        <service name="{@value}"/>
      </xsl:for-each>
    </parent-provides>

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

            <!-- missing logic to deal with other routes -->
            
          </xsl:choose>
        </xsl:for-each>

      </default-route>
    </xsl:for-each>


    <xsl:for-each select="expr/attrs/attr[@name='children']/attrs/attr">
      <start name="{@name}">
        
        <xsl:for-each select="attrs/attr[@name='binary']">
          <!-- include <binary/> if the name and binary are different -->
          <xsl:if test="string/@value!=../../@name">
            <binary name="{string/@value}"/>
          </xsl:if>
        </xsl:for-each>

        <xsl:for-each select="attrs/attr[@name='resources']/list/attrs">
          <resource>
            <xsl:for-each select="attr">
              <xsl:attribute name="{@name}">
                <xsl:value-of select="string/@value"/>
              </xsl:attribute>
            </xsl:for-each>
          </resource>
        </xsl:for-each>

        <xsl:for-each select="attrs/attr[@name='provides']/list">
          <provides>
            <xsl:for-each select="attrs/attr/string">
              <service name="{@value}"/>
            </xsl:for-each>
          </provides>
        </xsl:for-each>

      </start>
    </xsl:for-each>
      
  </config>

  </xsl:template>

</xsl:stylesheet>
