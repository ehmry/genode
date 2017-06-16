#
# \brief  Genode RPC definitions for Nim
# \author Emery Hemingway
# \date   2017-05-30
#

#
# Copyright (C) 2017 Genode Labs GmbH
#
# This file is part of the Genode OS framework, which is distributed
# under the terms of the GNU Affero General Public License version 3.
#

type
  RpcEffect* = object of RootEffect ## Blocking RPC effect.
