#
# \brief  ROM client
# \author Emery Hemingway
# \date   2017-10-15
#

#
# Copyright (C) 2017 Genode Labs GmbH
#
# This file is part of the Genode OS framework, which is distributed
# under the terms of the GNU Affero General Public License version 3.
#

import genode, streams, xmlparser, xmltree

const RomH = "<rom_session/connection.h>"

type
  ConnectionBase {.
    importcpp: "Genode::Rom_connection", header: RomH.} = object
  Connection = Constructible[ConnectionBase]

  RomClient* = ref RomClientObj
  RomClientObj = object
    conn: Connection
    stream*: DataspaceStream

proc construct(c: Connection, label: cstring) {.
  importcpp: "#.construct(*_genode_env, @)", tags: [RpcEffect].}

proc destruct(c: Connection) {.tags: [RpcEffect],
  importcpp: "#.destruct()".}

proc dataspace(c: Connection): DataspaceCapability {.tags: [RpcEffect],
  importcpp: "#->dataspace()".}
  ## Return the current dataspace capability from the ROM server.

proc update(c: Connection) {.tags: [RpcEffect],
  importcpp: "#->update()".}

proc sigh(c: Connection; cap: SignalContextCapability) {.tags: [RpcEffect],
  importcpp: "#->sigh(#)".}

proc newRomClient*(label: string): RomClient =
  ## Open a new ROM connection.
  new result
  construct result.conn, label
  result.stream = newDataspaceStream(result.conn.dataspace)

proc close*(rom: RomClient) =
  close rom.stream
  destruct rom.conn

proc dataspace*(rom: RomClient): DataspaceCapability = rom.conn.dataspace
  ## Return the ROM dataspace. Use the stream object member whenever possible.

proc update*(rom: RomClient) =
  ## Update the ROM dataspace.
  update rom.conn
  rom.stream.update rom.conn.dataspace

proc sigh*(rom: RomClient; cap: SignalContextCapability) = rom.conn.sigh cap
  ## Register a capability to a signal handler at the ROM server.

proc xml*(rom: RomClient): XmlNode =
  ## Parse ROM content as XML.
  rom.stream.setPosition 0
  parseXml rom.stream
