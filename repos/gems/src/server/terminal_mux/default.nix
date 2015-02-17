{ linkComponent, compileCC, libc, libc_terminal, ncurses }:

linkComponent rec {
  name = "terminal_mux";
  libs = [ libc libc_terminal ncurses ];
  objects = map
    (src: compileCC {
      inherit src libs;
      includes = [ ../terminal_mux ];
    })
    [ ./main.cc ./ncurses.cc ];
}
