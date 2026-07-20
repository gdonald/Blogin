use v6.d;

use Blogin::Log;
use Blogin::Site;

unit module Blogin;

our constant VERSION = '0.0.1';

our sub build(
  IO()        :$src!,
  IO()        :$out!,
  Bool        :$drafts = False,
  Int         :$jobs = ($*KERNEL.cpu-cores // 1),
  Blogin::Log :$log = Blogin::Log.new,
  --> Bool
) is export {
  my $result = Blogin::Site::build(
    content => $src,
    out     => $out,
    :$drafts,
    :$jobs,
  );

  $log.verbose("wrote { $result.written.elems } pages to { $out }");

  True;
}
