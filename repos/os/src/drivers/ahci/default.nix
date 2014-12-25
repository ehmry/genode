{ linkComponent, compileCC, ahci, server }:

linkComponent {
  name = "ahci";
  libs = [ ahci server ];
}
