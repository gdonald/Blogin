use v6.d;

use Blogin::Log;

unit module Blogin;

our constant VERSION = '0.0.1';

our sub build(
  IO()        :$src!,
  IO()        :$out!,
  Bool        :$drafts = False,
  Blogin::Log :$log = Blogin::Log.new,
  --> Bool
) is export {
  $out.mkdir unless $out.d;

  $log.verbose("wrote {$out}");

  True;
}
