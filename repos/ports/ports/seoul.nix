{ preparePort, fetchFromGitHub, python2, binutils }:

let
  rev = "7b228f6a210b827b9be7c9e2d30cd4095a32b2ab";
in
preparePort {
  name = "seoul-${builtins.substring 0 7 rev}";

  src = fetchFromGitHub {
    owner = "alex-ab";
    repo = "seoul";
    inherit rev;
    sha256 = "0d29bch74drzhjsm00v1d8j5b3rhhxq49ja2fnhfgpilmdw5lby2";
  };

  # Code generation is not trivial.
  preferLocalBuild = false;

  inherit python2;
  buildInputs = [ binutils ];

  preInstall =
    ''
      echo "fix python version in code generator scripts ..."
      sed -i "s|/usr/bin/env python2|$python2/bin/python|" model/intel82576vf/genreg.py executor/build_instructions.py
      echo "call code generators ... takes a while ..."

      cd executor
      ./build_instructions.py > instructions.inc
      cd ../model/intel82576vf
      ./genreg.py reg_pci.py ../../include/model/intel82576vfpci.inc
      ./genreg.py reg_mmio.py ../../include/model/intel82576vfmmio.inc
      cd ../..
    '';
}
