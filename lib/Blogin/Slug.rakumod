use v6.d;

unit module Blogin::Slug;

our sub slugify(Str $text --> Str) is export {
  my $slug = $text.lc;

  $slug = $slug.subst(/ <-[ a..z 0..9 ]>+ /, '-', :g);
  $slug = $slug.subst(/ '-'+ /, '-', :g);
  $slug = $slug.subst(/ ^ '-' /, '').subst(/ '-' $ /, '');

  $slug;
}
