use v6.d;

use Template::HAML;
use Template::HAML::HelpersRole;
use Blogin::Markdown;
use Blogin::Markdown::Html;
use Blogin::Framework;
use Blogin::Nav;
use Blogin::Slug;

unit module Blogin::Layout;

# Neutralize any '--' run so interpolated text cannot terminate an HTML comment.
sub sanitize-comment(Str $text --> Str) {
  $text.subst('--', '- -', :g);
}

sub attr-escape(Str $text --> Str) {
  $text.trans([ '&', '<', '>', '"' ] => [ '&amp;', '&lt;', '&gt;', '&quot;' ]);
}

sub nav-node-for(@nodes, Str $path) {
  for @nodes -> $node {
    return $node if $node.path eq $path;

    with nav-node-for($node.children, $path) -> $found {
      return $found;
    }
  }

  Nil;
}

class ChromeView does Template::HAML::HelpersRole {
  has      %.site;
  has Str  $.section   = '';
  has Str  $.url       = '';
  has      @.nav;
  has Bool $.has-header  = False;
  has Bool $.has-sidebar = False;
  has Bool $.has-footer  = False;
  has Bool $.debug       = False;
  has Profile $.framework = Blogin::Framework::profile('none');

  method framework-class(Str $slot --> Str) {
    $!framework.class-for($slot);
  }

  method framework-stylesheet-tag(--> Str) {
    my $href = $!framework.stylesheet;

    $href.chars ?? "<link rel=\"stylesheet\" href=\"{ attr-escape($href) }\">" !! '';
  }

  method framework-script-tag(--> Str) {
    my $src = $!framework.script;

    $src.chars ?? "<script src=\"{ attr-escape($src) }\"></script>" !! '';
  }

  method site-title  { %!site<title> // '' }
  method section     { $!section }

  method section-label(--> Str) {
    with nav-node-for(@!nav, $!section) -> $node {
      return $node.label;
    }

    Blogin::Slug::humanize($!section.split('/').grep(*.chars).tail // '');
  }
  method url         { $!url }
  method has-header  { $!has-header }
  method has-sidebar { $!has-sidebar }
  method has-footer  { $!has-footer }
  method template-label { 'template: show' }

  method nav-nodes { @!nav }

  method nav-current(NavNode $node --> Bool) {
    $!section eq $node.path || $!section.starts-with($node.path ~ '/');
  }

  method debug-open(Str $label --> Str) {
    $!debug ?? "<!-- begin { sanitize-comment($label) } -->\n" !! '';
  }

  method debug-close(Str $label --> Str) {
    $!debug ?? "<!-- end { sanitize-comment($label) } -->\n" !! '';
  }
}

class View is ChromeView is export {
  has      $.post;
  has Str  $.body-html = '';
  has Bool $.show-dates = True;
  has Str  $.prev-url = '';
  has Str  $.prev-title = '';
  has Str  $.next-url = '';
  has Str  $.next-title = '';

  method title       { $!post.title }
  method date        { $!post.date-str }
  method description  { $!post.description }
  method show-dates  { $!show-dates }

  method post-nav-html {
    return '' unless $!prev-url.chars || $!next-url.chars;

    my $button = self.framework-class('post-nav-button') || 'blogin-btn';

    my $out = '<nav class="post-nav">';
    $out ~= '<a class="prev ' ~ $button ~ '" href="' ~ attr-escape($!prev-url) ~ '">'
      ~ '<span aria-hidden="true">&larr;</span> ' ~ attr-escape($!prev-title) ~ '</a>' if $!prev-url.chars;
    $out ~= '<a class="next ' ~ $button ~ '" href="' ~ attr-escape($!next-url) ~ '">'
      ~ attr-escape($!next-title) ~ ' <span aria-hidden="true">&rarr;</span></a>' if $!next-url.chars;
    $out ~= '</nav>';
    $out;
  }

  method body {
    return $!body-html unless self.debug;

    my $provenance = '<!-- source: '
      ~ sanitize-comment($!post.filename)
      ~ ' slug=' ~ sanitize-comment($!post.slug)
      ~ ' title=' ~ sanitize-comment($!post.title)
      ~ " -->\n";

    $provenance ~ $!body-html;
  }
}

class ListView is ChromeView is export {
  has      @.entries;
  has Int  $.page-number = 1;
  has Int  $.total-pages = 1;
  has Str  $.prev-url = '';
  has Str  $.next-url = '';
  has Bool $.index-dates = True;

  method template-label { 'template: index' }
  method posts       { @!entries }
  method page-number { $!page-number }
  method total-pages { $!total-pages }
  method index-dates { $!index-dates }

  method pagination-html {
    return '' unless $!prev-url.chars || $!next-url.chars;

    my $class = self.framework-class('pagination');
    my $attr  = $class.chars ?? " class=\"{ $class }\"" !! '';

    my $out = "<nav$attr>";
    $out ~= '<a class="prev" href="' ~ attr-escape($!prev-url) ~ '">newer</a>' if $!prev-url.chars;
    $out ~= '<a class="next" href="' ~ attr-escape($!next-url) ~ '">older</a>' if $!next-url.chars;
    $out ~= '</nav>';
    $out;
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

# Markdown body to HTML fragment + plain text. Concurrency-safe.
our sub render-parts(:$post!, Str :$framework = 'none', Bool :$highlight = False --> Hash) is export {
  my $document = Blogin::Markdown::parse($post.body);
  my $renderer = Blogin::Markdown::Html.new(
    framework => Blogin::Framework::profile($framework),
    :$highlight,
  );
  my $result = $renderer.render($document);

  %( html => $result.html, text => $result.text );
}

our sub render-body(:$post!, Str :$framework = 'none' --> Str) is export {
  render-parts(:$post, :$framework)<html>;
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
        :@nav = [],
  Bool  :$debug = False,
  Str   :$framework = 'none',
  Bool  :$show-dates = True,
  Str   :$prev-url = '',
  Str   :$prev-title = '',
  Str   :$next-url = '',
  Str   :$next-title = '',
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
    :@nav,
    :$body-html,
    :$debug,
    :$show-dates,
    :$prev-url,
    :$prev-title,
    :$next-url,
    :$next-title,
    framework => Blogin::Framework::profile($framework),
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
        :@nav = [],
  Str   :$framework = 'none',
  Bool  :$debug = False,
  Bool  :$show-dates = True,
  Str   :$prev-url = '',
  Str   :$prev-title = '',
  Str   :$next-url = '',
  Str   :$next-title = '',
  --> Str
) is export {
  my $body-html = render-body(:$post, :$framework);

  render-with-layout(
    :$post, :$body-html, :$layouts, :%site, :$section, :$url, :@nav, :$debug, :$framework, :$show-dates,
    :$prev-url, :$prev-title, :$next-url, :$next-title,
  );
}

our sub render-listing(
  IO()  :$layouts!,
  Str   :$section = '',
        :%site = %(),
        :@entries,
  Int   :$page-number = 1,
  Int   :$total-pages = 1,
  Str   :$prev-url = '',
  Str   :$next-url = '',
        :@nav = [],
  Bool  :$debug = False,
  Str   :$framework = 'none',
        :@templates = ['index'],
  Bool  :$index-dates = True,
  --> Str
) is export {
  my @paths = layout-search-paths($layouts, $section);

  my $template = @templates.first({ template-exists(@paths, $_) });

  die "required listing layout ({ @templates.join(', ') }) not found (searched { @paths.join(', ') })"
    without $template;

  die "required layout 'base.haml' not found (searched { @paths.join(', ') })"
    unless template-exists(@paths, 'base');

  my $view = ListView.new(
    :%site,
    :$section,
    :@nav,
    :@entries,
    :$page-number,
    :$total-pages,
    :$prev-url,
    :$next-url,
    :$debug,
    :$index-dates,
    framework => Blogin::Framework::profile($framework),
    has-header  => partial-exists(@paths, 'header'),
    has-sidebar => partial-exists(@paths, 'sidebar'),
    has-footer  => partial-exists(@paths, 'footer'),
  );

  my $haml = HAML.new(:search-paths(@paths));

  $haml.render(:file($template), :layout<base>, :context($view));
}
