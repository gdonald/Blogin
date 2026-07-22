use v6.d;

use Blogin::Slug;

unit module Blogin::Nav;

class NavNode is export {
  has Str $.name;
  has Str $.label;
  has Str $.path;
  has Str $.url;
  has Int $.order = 0;
  has     @.children;
}

sub nodes-in(IO::Path:D $dir, Str $prefix, %sections, Bool $clean-urls, Str $url-prefix) {
  my @nodes;

  for $dir.dir.grep(*.d).sort(*.basename) -> $sub {
    my $name = $sub.basename;
    my $path = $prefix.chars ?? "$prefix/$name" !! $name;
    my %config = (%sections{$path} // %()).hash;

    next if (%config<nav>:exists) && !%config<nav>;

    my $label    = %config<label> // Blogin::Slug::humanize($name);
    my $order    = (%config<order> // 0).Int;
    my $url      = $clean-urls ?? "$url-prefix/$path" !! "$url-prefix/$path/";
    my @children = nodes-in($sub, $path, %sections, $clean-urls, $url-prefix);

    @nodes.push(NavNode.new(:$name, :$label, :$path, :$url, :$order, :@children));
  }

  @nodes.sort({ ($^a.order <=> $^b.order) || ($^a.name leg $^b.name) }).Array;
}

our sub build-tree(IO() $content, :%sections = %(), Bool :$clean-urls = True, Str :$url-prefix = '' --> Array) is export {
  return [].Array unless $content.d;

  nodes-in($content, '', %sections, $clean-urls, $url-prefix);
}
