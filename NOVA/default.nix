# SPDX-FileCopyrightText: Emery Hemingway
#
# SPDX-License-Identifier: LicenseRef-Hippocratic-1.1

{ stdenv, buildPackages, fetchFromGitHub }:

let
  ARCH = if stdenv.isx86_32 then
    "x86_32"
  else if stdenv.isx86_64 then
    "x86_64"
  else
    null;
in if ARCH == null then
  null
else

  buildPackages.stdenv.mkDerivation rec {
    # Borrow the build host compiler,
    pname = "NOVA";
    version = "r10";
    inherit ARCH;

    src = fetchFromGitHub {
      owner = "alex-ab";
      repo = "NOVA";
      rev = "68c2fb1671e75d811a4787e35b0d0c6cc85815c0";
      sha256 = "06zxz8hvzqgp8vrh6kv65j0z1m3xfm2ac8ppkv6ql0rivm1rv07s";
    };

    enableParallelBuilding = true;

    makeFlags = [ "--directory build" ];

    preInstall = "export INS_DIR=$out";

    meta = with stdenv.lib;
      src.meta // {
        description =
          "The NOVA OS Virtualization Architecture is a project aimed at constructing a secure virtualization environment with a small trusted computing base.";
        homepage = "http://hypervisor.org/";
        license = licenses.gpl2;
        maintainers = [ maintainers.ehmry ];
      };

  }
