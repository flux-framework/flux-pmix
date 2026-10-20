.NOTPARALLEL:

SUBDIRS = . test

MAN_FILES = $(MAN7_FILES)

MAN7_FILES = \
	man7/flux-shell-pmix.7

RST_FILES = \
	$(MAN7_FILES:.7=.rst)

if ENABLE_DOCS
man_MANS = $(MAN7_FILES)
endif

SUFFIXES = .rst .7

sphinx_man = $(sphinx_man_$(V))
sphinx_man_ = $(sphinx_man_$(AM_DEFAULT_VERBOSITY))
sphinx_man_0 = @echo "  BUILD     manpages";

sphinx_html = $(sphinx_html_$(V))
sphinx_html_ = $(sphinx_html_$(AM_DEFAULT_VERBOSITY))
sphinx_html_0 = @echo "  BUILD     html";

sphinx_verbose_flags = $(sphinx_verbose_flags_$(V))
sphinx_verbose_flags_ = $(sphinx_verbose_flags_$(AM_DEFAULT_VERBOSITY))
sphinx_verbose_flags_0 =
sphinx_verbose_flags_1 = -v
sphinx_verbose_flags_2 = -vv

STDERR_DEVNULL = $(stderr_devnull_$(V))
stderr_devnull_ =  $(stderr_devnull_$(AM_DEFAULT_VERBOSITY))
stderr_devnull_0 = >/dev/null 2>&1

$(MAN_FILES): manpages.py conf.py $(RST_FILES)
	$(sphinx_man) \
	SPHINX_BUILDDIR=$(abs_builddir) $(PYTHON) \
		-m sphinx $(sphinx_verbose_flags) -b man $(srcdir) ./man \
		$(STDERR_DEVNULL)
	@echo "  MV	manpages"; \
	for sec in 7; do \
	  $(MKDIR_P) man$$sec && \
	  mv -f $(abs_builddir)/man/*.$$sec man$$sec/; \
	done

.PHONY: html
html: conf.py $(RST_FILES) $(GUIDE_FILES)
	$(sphinx_html) \
	SPHINX_BUILDDIR=$(abs_builddir) $(PYTHON) \
		-m sphinx $(sphinx_verbose_flags) -b html $(srcdir) ./html \
		$(STDERR_DEVNULL)

GUIDE_FILES = \
	guide/openpmix_interop.rst \
	guide/tracing.rst

EXTRA_DIST = \
	conf.py \
	manpages.py \
	index.rst \
	domainrefs.py \
	requirements.txt \
	_static/css/custom.css \
	$(RST_FILES) \
	$(GUIDE_FILES) \
	man7/index.rst

CLEANFILES = \
	$(MAN_FILES)

clean-local:
	-rm -rf man

distclean-local:
	-rm -rf doctrees
