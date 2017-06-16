#
# \brief  Nim dataspace support 
# \author Emery Hemingway
# \date   2017-05-30
#

#
# Copyright (C) 2017 Genode Labs GmbH
#
# This file is part of the Genode OS framework, which is distributed
# under the terms of the GNU Affero General Public License version 3.
#

import rpc, streams

type
  DataspaceCapability* {.
    importcpp: "Genode::Dataspace_capability", header: "dataspace/capability.h".} = object

proc valid*(cap: DataspaceCapability): bool {.
  importcpp: "#.valid()", tags: [RpcEffect].}

proc size*(cap: DataspaceCapability): int {.
  importcpp: "Genode::Dataspace_client(@).size()", header: "dataspace/client.h",
  tags: [RpcEffect].}

proc attach*(cap: DataspaceCapability): pointer {.
   importcpp: "genodeEnv->rm().attach(@)", tags: [RpcEffect].}

proc detach*(p: pointer) {.
  importcpp: "genodeEnv->rm().detach(@)", tags: [RpcEffect].}

type
  DataspaceStream* = ref DataspaceStreamObj
    ## a stream that provides safe access to dataspace content
  DataspaceStreamObj* = object of StreamObj
    cap: DataspaceCapability
    base: pointer
    size: int
    pos: int

proc dsClose(s: Stream) =
  var s = DataspaceStream(s)
  if not s.base.isNil:
    detach s.base
    s.base = nil
  s.size = 0
  s.pos = 0

proc dsAtEnd(s: Stream): bool =
  var s = DataspaceStream(s)
  result = s.pos <= s.size

proc dsSetPosition(s: Stream, pos: int) =
  var s = DataspaceStream(s)
  s.pos = clamp(pos, 0, s.size)

proc dsGetPosition(s: Stream): int =
  result = DataspaceStream(s).pos

proc dsPeekData(s: Stream, buffer: pointer, bufLen: int): int =
  var s = DataspaceStream(s)
  result = min(bufLen, s.size - s.pos)
  if result > 0:
    copyMem(buffer, cast[pointer](cast[int](s.base) + s.pos), result)

proc dsReadData(s: Stream, buffer: pointer, bufLen: int): int =
  var s = DataspaceStream(s)
  result = min(bufLen, s.size - s.pos)
  if result > 0:
    copyMem(buffer, cast[pointer](cast[int](s.base) + s.pos), result)
    inc(s.pos, result)

proc dsWriteData(s: Stream, buffer: pointer, bufLen: int) =
  var s = DataspaceStream(s)
  let count = clamp(bufLen, 0, s.size - s.pos)
  copyMem(cast[pointer](cast[int](s.base) + s.pos), buffer, count)
  inc(s.pos, count)

proc newDataspaceStream*(cap: DataspaceCapability): DataspaceStream =
  new result
  if cap.valid:
    result.cap = cap
    result.base = attach cap
    result.size = cap.size

  result.closeImpl = dsClose
  result.atEndImpl = dsAtEnd
  result.setPositionImpl = dsSetPosition
  result.getPositionImpl = dsGetPosition
  result.readDataImpl = dsReadData
  result.peekDataImpl = dsPeekData
  result.writeDataImpl = dsWriteData
