use v6.d;

unit module Blogin::Framework;

my %PROFILES =
  none => %(),
  bootstrap5 => %(
    table      => 'table',
    blockquote => 'blockquote',
    image      => 'img-fluid',
  );

class Profile is export {
  has Str $.name;
  has %.classes;

  method class-for(Str $slot --> Str) {
    %!classes{$slot} // '';
  }
}

our sub profile(Str $name = 'none' --> Profile) is export {
  die "unknown css-framework '$name'" unless %PROFILES{$name}:exists;

  Profile.new(:$name, classes => %PROFILES{$name});
}
