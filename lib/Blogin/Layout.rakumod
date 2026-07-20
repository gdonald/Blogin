use v6.d;

use Template::HAML;
use Template::HAML::HelpersRole;
use Blogin::Markdown;
use Blogin::Markdown::Html;
use Blogin::Framework;

unit module Blogin::Layout;

# Neutralize any '--' run so interpolated text cannot terminate an HTML comment.
sub sanitize-comment(Str $text --> Str) {
  $text.subst('--', '- -', :g);
}

class View does Template::HAML::HelpersRole is export {
  has      $.post;
  has      %.site;
  has Str  $.section   = '';
  has Str  $.url       = '';
  has Str  $.body-html = '';
  has Bool $.has-header  = False;
  has Bool $.has-sidebar = False;
  has Bool $.has-footer  = False;
  has Bool $.debug       = False;

  method site-title  { %!site<title> // '' }
  method title       { $!post.title }
  method date        { $!post.date.defined ?? $!post.date.Str !! '' }
  method description  { $!post.description }
  method section     { $!section }
  method url         { $!url }
  method has-header  { $!has-header }
  method has-sidebar { $!has-sidebar }
  method has-footer  { $!has-footer }

  method body {
    return $!body-html unless $!debug;

    my $provenance = '<!-- source: '
      ~ sanitize-comment($!post.filename)
      ~ ' slug=' ~ sanitize-comment($!post.slug)
      ~ ' title=' ~ sanitize-comment($!post.title)
      ~ " -->\n";

    $provenance ~ $!body-html;
  }

  method debug-open(Str $label --> Str) {
    $!debug ?? "<!-- begin { sanitize-comment($label) } -->\n" !! '';
  }

  method debug-close(Str $label --> Str) {
    $!debug ?? "<!-- end { sanitize-comment($label) } -->\n" !! '';
  }
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

# Pure: markdown body to an HTML fragment. Safe to run concurrently.
our sub render-body(:$post!, Str :$framework = 'none' --> Str) is export {
  my $document = Blogin::Markdown::parse($post.body);
  my $renderer = Blogin::Markdown::Html.new(framework => Blogin::Framework::profile($framework));

  $renderer.render($document).html;
}

# HAML layout wrap. Template::HAML's compile caches are not thread-safe, so
# callers that parallelize must serialize this step.
our sub render-with-layout(
  :$post!,
  Str   :$body-html!,
  IO()  :$layouts!,
        :%site = %(),
  Str   :$section = '',
  Str   :$url = '',
  Bool  :$debug = False,
  --> Str
) is export {
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
    :$debug,
    has-header  => partial-exists(@paths, 'header'),
    has-sidebar => partial-exists(@paths, 'sidebar'),
    has-footer  => partial-exists(@paths, 'footer'),
  );

  my $haml = HAML.new(:search-paths(@paths));

  $haml.render(:file<show>, :layout<base>, :context($view));
}

our sub render-post(
  :$post!,
  IO()  :$layouts!,
        :%site = %(),
  Str   :$section = '',
  Str   :$url = '',
  Str   :$framework = 'none',
  Bool  :$debug = False,
  --> Str
) is export {
  my $body-html = render-body(:$post, :$framework);

  render-with-layout(:$post, :$body-html, :$layouts, :%site, :$section, :$url, :$debug);
}
