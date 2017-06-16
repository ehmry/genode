#
# \brief  Nim ROM client
# \author Emery Hemingway
# \date   2017-05-30
#

#
# Copyright (C) 2017 Genode Labs GmbH
#
# This file is part of the Genode OS framework, which is distributed
# under the terms of the GNU Affero General Public License version 3.
#

import streams, xmlparser, xmltree

import rpc, dataspace

type
  RomConnection* = ref RomConnectionObj
  RomConnectionObj {.importcpp: "Nim::RomConnection", header: "nim/rom.h".} = object

  RomDataspaceCallback* = proc (cap: DataspaceCapability) {.noconv.}

proc initRomConnection(rom: RomConnection, label: cstring, callback: RomDataspaceCallback) {.
  importcpp: "#->construct(*genodeEnv, @)", tags: [RpcEffect].}

proc newRomConnection*(label: string, cb: proc (cap: DataspaceCapability) {.noconv.}): RomConnection =
  ## Open a new ROM connection and apply a callback to the
  ## initial datapace and any updates.
  new result
  initRomConnection(result, label, cb)

template newRomConnection*(label: string, cb: proc (stream: Stream)): RomConnection =
  ## Open new ROM connection with a callback that receives a Stream.
  let wrapper = proc (cap: DataspaceCapability) {.noconv.} =
    let s = newDataspaceStream cap
    cb(s)
    close s
  newRomConnection(label, wrapper)

template newRomConnection*(label: string, cb: proc (xml: XmlNode)): RomConnection =
  ## Open new ROM connection with a callback that receives XML.
  let wrapper = proc (stream: Stream) =
    try:
      cb(parseXml(stream))
    except XmlError:
      discard
  newRomConnection(label, wrapper)

proc close*(rom: RomConnection) {.
  importcpp: "#->destruct()".}
