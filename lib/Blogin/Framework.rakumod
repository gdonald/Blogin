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

my %STYLESHEETS =
  bootstrap5 => 'https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css',
  pico       => 'https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css',
  bulma      => 'https://cdn.jsdelivr.net/npm/bulma@1/css/bulma.min.css';

my %SCRIPTS =
  bootstrap5 => 'https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js';

my @KNOWN = %PROFILES.keys.sort;

class Profile is export {
  has Str $.name;
  has %.classes;
  has Str $.stylesheet = '';
  has Str $.script     = '';

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

  Profile.new(
    :$name,
    classes    => %PROFILES{$name},
    stylesheet => %STYLESHEETS{$name} // '',
    script     => %SCRIPTS{$name} // '',
  );
}
