proc shared_folder { } {
	return [get_cmd_arg_first --shared-folder "/dev/null"]
}
