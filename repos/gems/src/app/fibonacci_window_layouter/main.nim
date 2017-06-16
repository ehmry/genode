#
# \brief  Fibonacci rectangle window layouter
# \author Emery Hemingway
# \date   2017-05-30
#

#
# Copyright (C) 2017 Genode Labs GmbH
#
# This file is part of the Genode OS framework, which is distributed
# under the terms of the GNU Affero General Public License version 3.
#

import os, streams, strtabs, xmltree

import report, rom

type
  Point = object
    x, y: int
  Area = object
    h, w: int
  Rect = object
    point: Point
    area: Area

  Window = ref object
    id: int
    title: string
    label: string
    geometry: Rect
    origGeometry: Rect
    requestedSize: Area
    unmaximizedGeometry: Rect
    maximizedGeometry: Rect
    hasAlpha: bool
    hidden: bool
    resizeable: bool
    maximized: bool
    dragged: bool
    toppedCnt: int
    focusHistoryEntry: bool

var windows= newSeqOfCap[Window](16)

proc romCb(stream: Stream) =
  echo stream.readAll

let
  windowReporter =
    newReportConnection("window_layout")
  resizeReporter =
    newReportConnection("resize_request")
  focusReporter =
    newReportConnection("focus")

proc processWindowList(root: XmlNode) =
  var outputRoot = newElement("window_layout")

  for input in root.items():
    echo input
    var
      output = newElement("window")
    echo output
    let
      attrs = newStringTable(
        "id", input.attr("id"),
        "title", input.attr("title"),
        "xpos", "0",
        "ypos", "0",
        "width", "300",
        "height", "200",
        "topped", "0",
        "focused", "no",
        "maximizer", "yes",
        "closer", "yes",
        modeCaseInsensitive)
    echo attrs

    output.attrs = attrs
    echo $output
    outputRoot.add(output)

  echo "submitting report"
  windowReporter.submit($outputRoot)

let
  configRom =
    newRomConnection("config", romCb)
  windowListRom =
    newRomConnection("window_list", processWindowList)
  focusRequestRom =
    newRomConnection("focus_request", romCb)
  hoverRom =
    newRomConnection("hover", romCb)
  decoratorMarginsRom =
    newRomConnection("decorator_margins", romCb)

while true:
  const Timeout: int = 60000
  sleep(Timeout)
