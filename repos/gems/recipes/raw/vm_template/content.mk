content: machine.vbox machine.vdi

machine.vbox:
	cp $(REP_DIR)/run/sculpt/$@ $@

machine.vdi:
	gzip -cdfk $(REP_DIR)/run/sculpt/$@.gz > $@
