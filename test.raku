#!/usr/bin/env raku

use v6.d;

$*OUT.out-buffer = False;

%*ENV<AUTHOR_TESTING> = 1;

chdir $*PROGRAM.parent;

my $jobs = max(2, ($*KERNEL.cpu-cores // 2) - 2);

my @cmd = 'behave', '--parallel', $jobs.Str, '--parallel-mode', 'queue', |@*ARGS;

say "==> @cmd.join(' ')";

my $proc = run(|@cmd);

exit $proc.exitcode;
