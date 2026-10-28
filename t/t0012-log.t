#!/bin/sh

test_description='Exercise server log callback'

. `dirname $0`/sharness.sh

PMIXLOG=${FLUX_BUILD_DIR}/t/src/pmixlog
export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success '1n1p log to stderr lands in task stderr' '
	run_timeout 30 flux run \
		${PMIXLOG} "lorem ipsum" 2>err1.out >out1.out &&
	grep "lorem ipsum" err1.out &&
	test_must_fail grep "lorem ipsum" out1.out
'

test_expect_success '1n1p log to stdout lands in task stdout' '
	run_timeout 30 flux run \
		${PMIXLOG} --stdout "dolor sit" 2>err2.out >out2.out &&
	grep "dolor sit" out2.out &&
	test_must_fail grep "dolor sit" err2.out
'

test_expect_success '2n4p log message is written by every rank' '
	run_timeout 30 flux run -N2 -n4 --label-io \
		${PMIXLOG} "untagged" 2>err3.out &&
	for rank in 0 1 2 3; do grep "^$rank: untagged" err3.out; done
'

test_expect_success '2n4p tagged log message is written once' '
	run_timeout 30 flux run -N2 -n4 \
		${PMIXLOG} --key=help-foo.txt --val=some-topic --count=3 \
		"tagged" 2>err4.out &&
	test $(grep -c "^tagged" err4.out) -eq 1 &&
	grep "1 more node sent help message help-foo.txt / some-topic" err4.out
'

test_expect_success '2n2p distinct tags are each written once' '
	run_timeout 30 flux run -N2 -n2 \
		${PMIXLOG} --key=help-foo.txt --val=topic-a "tag-a" 2>err5a.out &&
	test $(grep -c "^tag-a" err5a.out) -eq 1 &&
	run_timeout 30 flux run -N2 -n2 \
		${PMIXLOG} --key=help-foo.txt --val=topic-b "tag-b" 2>err5b.out &&
	test $(grep -c "^tag-b" err5b.out) -eq 1
'

test_expect_success '2n2p tagged log from rank 1 only is written' '
	run_timeout 30 flux run -N2 -n2 --label-io \
		${PMIXLOG} --rank=1 --key=help-foo.txt --val=topic-c \
		"from-rank-1" 2>err6.out &&
	grep "^1: from-rank-1" err6.out &&
	test_must_fail grep "more node" err6.out
'

# openpmix drops repeats of a tag before the log callback is reached,
# so without --no-agg the shell is handed one message per node and its
# own per shell de-duplication never runs.  PMIX_LOG_AGG false turns
# that off, which both exercises that code and makes the counts exact
# rather than a range that moves with the openpmix version.
#
# The attribute is honored by the server, which here is the shell, so
# whether it works depends on the openpmix the plugin is built against.
# Check once and skip the exact cases where it does not.
test_expect_success 'probe whether openpmix honors PMIX_LOG_AGG' '
	run_timeout 30 flux run -N1 -n1 -o pmix.log-dedup=off \
		${PMIXLOG} --no-agg --key=help-agg.txt --val=probe --count=3 \
		"agg-probe" 2>agg.out
'

if test $(grep -c "^agg-probe" agg.out 2>/dev/null) = 3; then
	test_set_prereq LOG_AGG
fi

test_expect_success LOG_AGG '1n2p repeats within a shell are written once' '
	run_timeout 30 flux run -N1 -n2 \
		${PMIXLOG} --no-agg --key=help-foo.txt --val=topic-s \
		"tag-s" 2>err6s.out &&
	test $(grep -c "^tag-s" err6s.out) -eq 1 &&
	test_must_fail grep "more node" err6s.out
'

test_expect_success LOG_AGG '1n1p repeats from one rank are written once' '
	run_timeout 30 flux run -N1 -n1 \
		${PMIXLOG} --no-agg --key=help-foo.txt --val=topic-r --count=3 \
		"tag-r" 2>err6r.out &&
	test $(grep -c "^tag-r" err6r.out) -eq 1
'

test_expect_success LOG_AGG '2n4p tagged log message is written once' '
	run_timeout 30 flux run -N2 -n4 \
		${PMIXLOG} --no-agg --key=help-foo.txt --val=topic-j \
		"tag-j" 2>err6j.out &&
	test $(grep -c "^tag-j" err6j.out) -eq 1 &&
	grep "1 more node sent help message help-foo.txt / topic-j" err6j.out
'

test_expect_success LOG_AGG '2n4p every rank is written with pmix.log-dedup=off' '
	run_timeout 30 flux run -N2 -n4 -o pmix.log-dedup=off \
		${PMIXLOG} --no-agg --key=help-foo.txt --val=topic-d \
		"tag-d" 2>err6b.out &&
	test $(grep -c "^tag-d" err6b.out) -eq 4 &&
	test_must_fail grep "more node" err6b.out
'

test_expect_success 'pmix.log-dedup rejects an unknown value' '
	test_must_fail run_timeout 30 flux run -o pmix.log-dedup=42 \
		${PMIXLOG} "x" 2>err6c.out &&
	grep "pmix.log-dedup must be .on. or .off." err6c.out
'

test_expect_success '1n1p log to syslog lands in the shell log at ERROR' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.syslog "to syslog" 2>err7.out &&
	grep "ERROR: pmix: rank 0: to syslog" err7.out
'

test_expect_success '1n1p log to local and global syslog work' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.lsys "to lsys" 2>err7a.out &&
	grep "ERROR: pmix: rank 0: to lsys" err7a.out &&
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.gsys "to gsys" 2>err7b.out &&
	grep "ERROR: pmix: rank 0: to gsys" err7b.out
'

test_expect_success '1n1p syslog priority sets the shell log level' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.syslog --priority=4 "warn" 2>err7c.out &&
	grep "WARN: pmix: rank 0: warn" err7c.out
'

test_expect_success '1n1p syslog at debug priority is hidden by default' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.syslog --priority=7 "dbg" 2>err7d.out &&
	test_must_fail grep "rank 0: dbg" err7d.out &&
	run_timeout 30 flux run -o verbose=2 \
		${PMIXLOG} --channel=pmix.log.syslog --priority=7 "dbg" 2>err7e.out &&
	grep "TRACE: pmix: rank 0: dbg" err7e.out
'

test_expect_success '1n1p out of range syslog priority is logged at ERROR' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.syslog --priority=99 "oops" 2>err7f.out &&
	grep "ERROR: pmix: rank 0: oops" err7f.out
'

test_expect_success '1n1p log to job record lands in the shell log' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.jrec "for the record" 2>err7l.out &&
	grep "pmix: rank 0: for the record" err7l.out &&
	test_must_fail grep "ERROR: pmix: rank 0: for the record" err7l.out
'

test_expect_success '1n1p job record is in the output eventlog' '
	jobid=$(flux submit ${PMIXLOG} --channel=pmix.log.jrec "recorded") &&
	flux job wait-event -t 30 $jobid clean &&
	flux job eventlog -p guest.output $jobid | grep "recorded"
'

test_expect_success '1n1p same message to syslog and job record is written once' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.syslog --channel=pmix.log.jrec \
		--priority=4 "not twice" 2>err7m.out &&
	test $(grep -c "rank 0: not twice" err7m.out) -eq 1 &&
	grep "WARN: pmix: rank 0: not twice" err7m.out
'

test_expect_success '1n1p job record before syslog still uses the priority' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.jrec --channel=pmix.log.syslog \
		--priority=4 "not twice" 2>err7n.out &&
	test $(grep -c "rank 0: not twice" err7n.out) -eq 1 &&
	grep "WARN: pmix: rank 0: not twice" err7n.out
'

test_expect_success '1n1p one call can log to several channels' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.stderr --channel=pmix.log.stdout \
		"both" 2>err7g.out >out7g.out &&
	grep "^both" err7g.out &&
	grep "^both" out7g.out
'

test_expect_success '1n1p unsupported log channel fails with NOT_SUPPORTED' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.email --expect=PMIX_ERR_NOT_SUPPORTED \
		"to email" 2>err7h.out &&
	test_must_fail grep "to email" err7h.out
'

test_expect_success '1n1p supported channel is written when another fails' '
	run_timeout 30 flux run \
		${PMIXLOG} --channel=pmix.log.email --channel=pmix.log.stderr \
		--expect=PMIX_ERR_NOT_SUPPORTED "mixed" 2>err7i.out &&
	grep "^mixed" err7i.out
'

test_expect_success '1n1p PMIX_LOG_ONCE logs to the first supported channel' '
	run_timeout 30 flux run \
		${PMIXLOG} --once --channel=pmix.log.email --channel=pmix.log.stderr \
		--channel=pmix.log.stdout "once" 2>err7j.out >out7j.out &&
	grep "^once" err7j.out &&
	test_must_fail grep "^once" out7j.out
'

test_expect_success '1n1p optional directive is accepted' '
	run_timeout 30 flux run \
		${PMIXLOG} --timestamp "ts" 2>err7k.out &&
	grep "^ts" err7k.out
'

test_done
