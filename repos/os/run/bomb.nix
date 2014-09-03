/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool, base, os }:

tool.run {
  name = "bomb";

  bootInputs = [ base.core os.init os.driver.timer os.test.bomb ];

  config = tool.initConfig {
    prioLevels = 2;
    parentProvides =
      [ "ROM" "RAM" "IRQ" "IO_MEM"
        "IO_PORT" "CAP" "PD" "RM"
        "CPU" "LOG" "SIGNAL"
      ];

    defaultRoute = [ "any-service" "parent" "any-child" ];

    children = {
      "timer" =
        { binary = os.driver.timer.name;
          resources = [ { name="RAM"; quantum="768K"; } ];
          provides  = [ { name="Timer"; } ];
        };
      "bomb" =
        { #binary = os.test.bomb;
	  resource = { name="RAM"; quantum="2G"; };
	  config = { rounds="20"; };
        };
    };
  };

  waitRegex = "Done\. Going to sleep\.";
  waitTimeout = 300;
}