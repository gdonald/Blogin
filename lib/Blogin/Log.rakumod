use v6.d;

unit class Blogin::Log;

my %RANK = quiet => 0, normal => 1, verbose => 2;

has Str        $.level = 'normal';
has IO::Handle $.out   = $*OUT;
has IO::Handle $.err   = $*ERR;

submethod TWEAK {
  die "unknown log level '$!level'" unless %RANK{$!level}:exists;
}

method !rank(--> Int) { %RANK{$!level} }

method info(Str() $message --> Nil) {
  $!out.say($message) if self!rank >= %RANK<normal>;
}

method verbose(Str() $message --> Nil) {
  $!out.say($message) if self!rank >= %RANK<verbose>;
}

method warn(Str() $message --> Nil) {
  $!err.say($message) if self!rank >= %RANK<normal>;
}

method error(Str() $message --> Nil) {
  $!err.say($message);
}
