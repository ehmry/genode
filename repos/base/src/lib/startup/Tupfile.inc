# This file must be included from another "repository" because
# the C preprocessor is injecting headers that have the same
# filename across "repositories", and if those headers are included
# from this "repository", then things break at runtime. Thus the
# preprocessor include directories must be set correctly before
# this file is included. Do not be fooled into thinking that the
# directory structure has significance.

: foreach $(BASE_DIR)/src/lib/startup/*.cc |> !cxx |> | $(REP_DIR)/<startup> {obj}

ifeq (@(TUP_ARCH),i386)
	: foreach $(BASE_DIR)/src/lib/startup/spec/x86_32/*.s \
	|> !asm |> | $(REP_DIR)/<startup> {obj}
endif
ifeq (@(TUP_ARCH),x86_64)
	: foreach $(BASE_DIR)/src/lib/startup/spec/x86_64/*.s \
	|> !asm |> | $(REP_DIR)/<startup> {obj}
endif
ifeq (@(TUP_ARCH),arm64)
	: foreach $(BASE_DIR)/src/lib/startup/spec/arm_64/*.s \
	|> !asm |> | $(REP_DIR)/<startup> {obj}
endif
