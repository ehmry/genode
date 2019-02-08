VERSION = 11.0.03

content: unifont.ttf LICENSE

unifont.ttf:
	wget http://unifoundry.com/pub/unifont/unifont-$(VERSION)/font-builds/unifont-$(VERSION).ttf -O $@

LICENSE:
	wget http://unifoundry.com/LICENSE.txt -O $@
