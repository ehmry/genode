{ tool, callComponent, ... }:

builtins.removeAttrs (tool.loadExpressions (callComponent {}) ./src) [ "lib" ]