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
  Int            :$jobs = ($*KERNEL.cpu-cores // 1),
  Bool           :$debug,
  Blogin::Log    :$log = Blogin::Log.new,
  --> Bool
) is export {
  my $out-dir = $out // $config.output-dir.IO;
  my $debug-on = $debug // $config.debug;

  my $result = Blogin::Site::build(
    content      => $src,
    out          => $out-dir,
    site         => %( title => $config.title, base-url => $config.base-url, author => $config.author ),
    clean-urls   => $config.clean-urls,
    framework    => $config.css-framework,
    page-size    => $config.page-size,
    home-section => $config.home-section,
    sections     => $config.sections,
    debug        => $debug-on,
    :$drafts,
    :$jobs,
  );

  $log.verbose("wrote { $result.written.elems } pages to { $out-dir }");

  True;
}
