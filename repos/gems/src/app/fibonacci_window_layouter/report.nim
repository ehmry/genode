#
# \brief  Nim Report client
# \author Emery Hemingway
# \date   2017-05-30
#

#
# Copyright (C) 2017 Genode Labs GmbH
#
# This file is part of the Genode OS framework, which is distributed
# under the terms of the GNU Affero General Public License version 3.
#

import rpc

type
  ReportConnection* = ref ReportConnectionObj
  ReportConnectionObj {.importcpp: "Nim::ReportConnection", header: "nim/report.h".} = object

proc initReportConnection(report: ReportConnection, label: cstring, bufferSize: int) {.
  importcpp: "#->construct(*genodeEnv, @)", tags: [RpcEffect].}

proc submit(report: ReportConnection, data: cstring, len: int) {.
  importcpp: "(*#)->submit(@)", tags: [RpcEffect].}

proc newReportConnection*(label: string, bufferSize = 4096): ReportConnection=
  new result
  result.initReportConnection(label, bufferSize)

proc submit*(report: ReportConnection, data: string) =
  report.submit(data, data.len+1)
  
proc close*(report: ReportConnection) {.
  importcpp: "#->destruct()".}
