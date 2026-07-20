use v6.d;

unit module Blogin::Framework;

my %PROFILES =
  none => %(),
  bootstrap5 => %(
    table      => 'table',
    blockquote => 'blockquote',
    image      => 'img-fluid',
    pagination => 'pagination',
    nav        => 'nav',
    container  => 'container',
  ),
  pico => %(
    container => 'container',
  ),
  bulma => %(
    table       => 'table',
    image       => 'image',
    article     => 'content',
    pagination  => 'pagination',
    nav         => 'navbar',
    tag         => 'tag',
    container   => 'container',
  );

my @KNOWN = %PROFILES.keys.sort;

class Profile is export {
  has Str $.name;
  has %.classes;

  method class-for(Str $slot --> Str) {
    %!classes{$slot} // '';
  }
}

our sub known(--> List) is export {
  @KNOWN;
}

our sub profile(Str $name = 'none' --> Profile) is export {
  die "unknown css-framework '$name' (known: { @KNOWN.join(', ') })"
    unless %PROFILES{$name}:exists;

  Profile.new(:$name, classes => %PROFILES{$name});
}
