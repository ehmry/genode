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
    name = "NOVA";
    inherit ARCH;

    src = fetchFromGitHub {
      owner = "alex-ab";
      repo = "NOVA";
      rev = "0ebcb4fc5a25d1df4451a89cbc87d88e099acbd3";
      sha256 = "0rkp59496032kq8a3l5fs771m5f7s5yywkxjk7j9qhmsidgk40wd";
    };

    enableParallelBuilding = true;

    makeFlags = [ "--directory=build" ];

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
