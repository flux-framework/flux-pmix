SUBDIRS = src .

AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_TESTS_ENVIRONMENT = \
	test -n "$$FLUX_TEST_INSTALLED_PATH" && \
	export PYTHONPATH="$(PYTHON_SITE_PKG):$$PYTHONPATH";\
	export PYTHON="${PYTHON}";\
	export PATH="$(FLUX_PREFIX)/bin:$(PATH)";

TEST_EXTENSIONS = .t .py
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh
PY_LOG_DRIVER = $(PYTHON) $(top_srcdir)/config/tap-driver.py

clean-local:
	rm -fr trash-directory.* test-results .prove *.broker.log */*.broker.log *.output python/__pycache__

# This list is included in both TESTS and dist_check_SCRIPTS.
TESTSCRIPTS = \
	t0000-sharness.t \
	t0002-basic.t \
	t0003-barrier.t \
	t0004-bizcard.t \
	t0005-abort.t \
	t0006-notify.t \
	t0007-dmodex.t \
	t0008-upmi.t \
	t0009-localhost.t \
	t0010-capture.t \
	t0011-stdkeys.t \
	t1000-ompi-basic.t \
	t1002-ompi-abort.t

# make check runs these TAP tests directly (both scripts and programs)
TESTS = \
	$(TESTSCRIPTS)

dist_check_SCRIPTS = \
	$(TESTSCRIPTS)

check-prep:
	$(MAKE)

EXTRA_DIST= \
	aggregate-results.sh \
	sharness.sh \
	sharness.d \
	$(T) \
	scripts/run_timeout.py \
	etc/rc.lua.in
