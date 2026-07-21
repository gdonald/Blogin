use v6.d;

use Blogin::Log;
use Blogin::Site;
use Blogin::Config;

unit module Blogin;

our constant VERSION = '0.0.1';

our sub build(
  IO()           :$src!,
  Blogin::Config :$config = Blogin::Config.new,
  IO()           :$out,
  Bool           :$drafts = False,
  Bool           :$future = False,
  Int            :$jobs = ($*KERNEL.cpu-cores // 1),
  Bool           :$debug,
  Bool           :$force = False,
  Blogin::Log    :$log = Blogin::Log.new,
  --> Bool
) is export {
  my $out-dir = $out // $config.output-dir.IO;

  my $result = Blogin::Site::build(
    content => $src,
    out     => $out-dir,
    debug   => ($debug // $config.debug),
    |$config.build-options,
    :$drafts,
    :$future,
    :$jobs,
    :$force,
  );

  $log.verbose("wrote { $result.written.elems } pages to { $out-dir }");

  True;
}

our sub clean(
  IO()        :$out!,
  IO()        :$root = $*CWD,
  Blogin::Log :$log = Blogin::Log.new,
  --> Int
) is export {
  my $removed = Blogin::Site::clean(:$out, :$root);

  $log.verbose("cleaned { $out } ($removed files)");

  $removed;
}
