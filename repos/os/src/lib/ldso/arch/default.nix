/*
 * \author Emery Hemingway
 * \date   2014-09-22
 */

{ tool }: with tool;
{ }:

let
  srcdir = if tool.build.isLinux then ./linux else ../arch;
in
buildLibrary {
  name = "ldso-arch";
  sources = map (fn: "${srcdir}/${fn}")
    [ "parent_cap.cc"
      "binary_name.cc"
    ];
}