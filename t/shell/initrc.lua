-- Job shell initrc for the testsuite, set as conf.shell_initrc by
-- test_under_flux.  It is the stock initrc without its
--   plugin.load { file = "*.so" }
-- of the system plugin directory.  That would load an installed
-- flux-pmix, and its libpmix.so.2 with it.  Loading this tree's pmix.so
-- afterwards replaces the plugin by name, but the dynamic linker reuses
-- the libpmix already in the process, so the tree's plugin would run on
-- the installed openpmix.  The tree's pmix.so is loaded by t/etc/rc.lua,
-- found through FLUX_SHELL_RC_PATH.
--
-- The system lua.d is not sourced either: the testsuite should not
-- depend on what is installed.  The rcpath is this directory's lua.d,
-- which does not exist, followed by FLUX_SHELL_RC_PATH.

shell.source_rcpath ("*.lua")
shell.source_rcpath_option ("mpi")
if shell.options.userrc then source (shell.options.userrc) end
