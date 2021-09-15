AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)
AM_CPPFLAGS = \
	-I$(top_srcdir)

shell_plugin_LTLIBRARIES = \
	pmix.la

pmix_la_SOURCES = \
	main.c
pmix_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(FLUX_CORE_CFLAGS) \
	$(FLUX_IDSET_CFLAGS) \
	$(FLUX_HOSTLIST_CFLAGS) \
	$(PMIX_CFLAGS) \
	$(JANSSON_CFLAGS)
pmix_la_LIBADD = \
	$(FLUX_CORE_LIBS) \
	$(PMIX_LIBS) \
	$(FLUX_IDSET_LIBS) \
	$(FLUX_HOSTLIST_LIBS) \
	$(JANSSON_LIBS)
pmix_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(fluxplugin_ldflags) \
	-module
