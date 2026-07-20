use v6.d;

use Template::HAML;
use Blogin::Markdown;
use Blogin::Markdown::Html;
use Blogin::Framework;

unit module Blogin::Layout;

class View is export {
  has      $.post;
  has      %.site;
  has Str  $.section   = '';
  has Str  $.url       = '';
  has Str  $.body-html = '';
  has Bool $.has-header  = False;
  has Bool $.has-sidebar = False;
  has Bool $.has-footer  = False;

  method site-title  { %!site<title> // '' }
  method title       { $!post.title }
  method body        { $!body-html }
  method date        { $!post.date.defined ?? $!post.date.Str !! '' }
  method description  { $!post.description }
  method section     { $!section }
  method url         { $!url }
  method has-header  { $!has-header }
  method has-sidebar { $!has-sidebar }
  method has-footer  { $!has-footer }
}

sub template-exists(@paths, Str $name --> Bool) {
  for @paths -> $dir {
    return True if "$dir/$name.haml".IO.e || "$dir/$name.html.haml".IO.e;
  }
  False;
}

sub partial-exists(@paths, Str $name --> Bool) {
  for @paths -> $dir {
    return True if "$dir/_$name.haml".IO.e || "$dir/_$name.html.haml".IO.e;
  }
  False;
}

sub layout-search-paths(IO() $layouts, Str $section --> Array) {
  my @paths;

  if $section.chars {
    my @segments = $section.split('/').grep(*.chars);

    for (1 .. @segments.elems).reverse -> $depth {
      @paths.push($layouts.add(@segments[^$depth].join('/')).Str);
    }
  }

  @paths.push($layouts.Str);
  @paths;
}

our sub render-post(
  :$post!,
  IO()  :$layouts!,
        :%site = %(),
  Str   :$section = '',
  Str   :$url = '',
  Str   :$framework = 'none',
  --> Str
) is export {
  my $document  = Blogin::Markdown::parse($post.body);
  my $renderer  = Blogin::Markdown::Html.new(framework => Blogin::Framework::profile($framework));
  my $body-html = $renderer.render($document).html;

  my @paths = layout-search-paths($layouts, $section);

  die "required layout 'show.haml' not found (searched { @paths.join(', ') })"
    unless template-exists(@paths, 'show');

  die "required layout 'base.haml' not found (searched { @paths.join(', ') })"
    unless template-exists(@paths, 'base');

  my $view = View.new(
    :$post,
    :%site,
    :$section,
    :$url,
    :$body-html,
    has-header  => partial-exists(@paths, 'header'),
    has-sidebar => partial-exists(@paths, 'sidebar'),
    has-footer  => partial-exists(@paths, 'footer'),
  );

  my $haml = HAML.new(:search-paths(@paths));

  $haml.render(:file<show>, :layout<base>, :context($view));
}
