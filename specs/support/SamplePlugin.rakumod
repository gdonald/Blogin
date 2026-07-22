use v6.d;

unit module SamplePlugin;

our sub blogin-emit(%context) {
  my $file = %context<out>.add('plugin-output.txt');
  $file.spurt("pages: { %context<pages>.elems }\n");
}
