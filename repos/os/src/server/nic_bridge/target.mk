TARGET      = nic_bridge
LIBS        = base net
SRC_CC      = component.cc main.cc packet_handler.cc uplink.cc
CONFIG_XSD  = config.xsd
INC_DIR    += $(PRG_DIR)
