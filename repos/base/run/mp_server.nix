#
# \brief  Test to start and call RPC entrypoint on all available CPUs
# \author Norman Feske
# \author Alexander Boettcher
#

{ spec, run, pkgs }:

#if {
#	![have_spec hw_arndale] &&
#	([have_spec platform_pbxa9] || (![have_spec nova] && ![have_spec foc]))
#} {
#	puts "Platform is unsupported."
#	exit 0
#}

if spec.kernel != "nova" then null else

run {
  name = "mp_server";

  contents = [
    { target = "/"; source = pkgs.test.mp_server; }
    { target = "/config";
      source = builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="LOG"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="CAP"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-mp_server">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
      '';
    }
  ];

  testScript = ''
if {[is_qemu_available]} {
	set want_cpus 2
	append qemu_args " -nographic -m 64 -smp $want_cpus,cores=$want_cpus "
}

# run the test
run_genode_until {\[init -\> test-mp_server\] done.*\n} 60

set cpus [regexp -inline {Detected [0-9x]+ CPU[ s]\.} $output]
set cpus [regexp -all -inline {[0-9]+} $cpus]
set cpus [expr [lindex $cpus 0] * [lindex $cpus 1]]

if {[is_qemu_available]} {
	if {$want_cpus != $cpus} {
		puts "CPU count is not as expected: $want_cpus != $cpus"
		exit 1;
	}
}

# pay only attention to the output of init and its children
grep_output {^\[init -\>}

unify_output {transfer cap [a-f0-9]+} "transfer cap UNIFIED"
unify_output {yes - idx [a-f0-9]+} "yes - idx UNIFIED"
unify_output {\- received cap [a-f0-9]+} "- received cap UNIFIED"

set good_string {
	[init -> test-mp_server] --- test-mp_server started ---
	[init -> test-mp_server] Detected }
append good_string "$cpus"
append good_string "x1 CPUs.\n"

for {set r 0} {$r < $cpus} {incr r} {
	append good_string {[init -> test-mp_server] call server on CPU }
	append good_string "$r\n"
	append good_string {[init -> test-mp_server] function test_untyped: got value }
	append good_string "$r\n"
}

for {set r 0} {$r < $cpus} {incr r} {
	append good_string {[init -> test-mp_server] call server on CPU }
	append good_string "$r - transfer cap UNIFIED\n"
	append good_string {[init -> test-mp_server] function test_cap: capability is valid ? yes - idx UNIFIED}
	append good_string "\n"
}

for {set r 0} {$r < $cpus} {incr r} {
	append good_string {[init -> test-mp_server] call server on CPU }
	append good_string "$r - transfer cap UNIFIED\n"
	append good_string {[init -> test-mp_server] function test_cap_reply: capability is valid ? yes - idx UNIFIED}
	append good_string "\n"
	append good_string {[init -> test-mp_server] got from server on CPU }
	append good_string "$r - received cap UNIFIED\n"
}

append good_string {[init -> test-mp_server] done}

compare_output_to $good_string
'';
}
