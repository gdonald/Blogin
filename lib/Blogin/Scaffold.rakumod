use v6.d;

use Blogin::Slug;

unit module Blogin::Scaffold;

my %STYLESHEETS =
  bootstrap5 => 'https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css',
  pico       => 'https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css',
  bulma      => 'https://cdn.jsdelivr.net/npm/bulma@1/css/bulma.min.css';

my @KNOWN-FRAMEWORKS = <none bootstrap5 pico bulma>;

sub blogin-json(Str $framework --> Str) {
  qq:to/JSON/;
  \{
    "title": "My Blogin Site",
    "base-url": "https://example.com",
    "home-section": "posts",
    "clean-urls": true,
    "css-framework": "$framework",
    "page-size": 10
  \}
  JSON
}

sub starter-post(Str $date --> Str) {
  qq:to/POST/;
  ---
  title: Hello World
  date: $date
  tags: [intro]
  description: The first post on your Blogin site.
  ---
  Welcome to your new **Blogin** site.

  Edit the Markdown under `content/`, tweak the HAML in `layouts/`, then run
  `blogin build` to regenerate `public/`.
  POST
}

sub base-haml(Str $framework --> Str) {
  my $stylesheet = %STYLESHEETS{$framework} // '/static/style.css';

  my $template = q:to/HAML/;
  !!! 5
  %html{lang: 'en'}
    %head
      %meta{charset: 'utf-8'}
      %meta{name: 'viewport', content: 'width=device-width, initial-scale=1'}
      %title= site-title
      %link{rel: 'stylesheet', href: 'STYLESHEET_HREF'}
    %body
      - if has-header
        != debug-open('partial: header')
        != render(:partial<header>, :locals(%( brand => site-title )))
        != debug-close('partial: header')
      .layout
        - if has-sidebar
          %aside
            != render(:partial<sidebar>)
        %main
          != debug-open(template-label)
          = yield
          != debug-close(template-label)
      - if has-footer
        != debug-open('partial: footer')
        != render(:partial<footer>)
        != debug-close('partial: footer')
  HAML

  $template.subst('STYLESHEET_HREF', $stylesheet);
}

sub show-haml(--> Str) {
  q:to/HAML/;
  %article
    %h1= title
    %p.meta= date
    != body
  HAML
}

sub index-haml(--> Str) {
  q:to/HAML/;
  %section.listing
    %h1= section
    %ul
      != render(:partial<entry>, :collection(posts), :as<entry>)
    != pagination-html
  HAML
}

sub entry-haml(--> Str) {
  q:to/HAML/;
  %li
    %a{href: "#{$entry<url>}"}= $entry<title>
    %span.date= $entry<date>
  HAML
}

sub header-haml(--> Str) {
  q:to/HAML/;
  %header
    %a.brand{href: '/'}= $brand
    != render(:partial<nav>)
  HAML
}

sub nav-haml(--> Str) {
  q:to/HAML/;
  %nav
    %ul
      != render(:partial<nav-item>, :collection(nav-nodes), :as<node>)
  HAML
}

sub nav-item-haml(--> Str) {
  q:to/HAML/;
  %li
    - if nav-current($node)
      %a.current{href: "#{$node.url}"}= $node.label
    - else
      %a{href: "#{$node.url}"}= $node.label
    - if $node.children.elems
      %ul
        != render(:partial<nav-item>, :collection($node.children), :as<node>)
  HAML
}

sub sidebar-haml(--> Str) {
  q:to/HAML/;
  %section.sidebar
    != render(:partial<search>)
  HAML
}

sub footer-haml(--> Str) {
  q:to/HAML/;
  %footer
    %p Built with Blogin.
  HAML
}

sub search-haml(--> Str) {
  q:to/HAML/;
  %form{'data-blogin-search' => 'true'}
    %input{type: 'search', name: 'q', placeholder: 'Search'}
  %ul{'data-blogin-results' => 'true'}
  %script{src: '/search.js'}
  HAML
}

sub style-css(--> Str) {
  q:to/CSS/;
  body { font-family: system-ui, sans-serif; margin: 0; line-height: 1.5; }
  .layout { display: flex; gap: 2rem; max-width: 60rem; margin: 0 auto; padding: 1rem; }
  main { flex: 1; }
  aside { width: 14rem; }
  nav ul { list-style: none; padding: 0; display: flex; gap: 1rem; }
  .current { font-weight: bold; }
  pre { background: #f5f5f5; padding: 1rem; overflow-x: auto; }
  .hl-keyword { color: #d73a49; }
  .hl-string { color: #032f62; }
  .hl-number { color: #005cc5; }
  .hl-comment { color: #6a737d; font-style: italic; }
  CSS
}

sub post-stub(Str $title, Str $date --> Str) {
  qq:to/POST/;
  ---
  title: "$title"
  date: $date
  tags: []
  description:
  ---
  Write your post here.
  POST
}

our sub new-post(
  Str   $title,
  IO()  :$content!,
  Str   :$section = '',
  Str   :$date = Date.today.Str,
  Bool  :$force = False,
  --> IO::Path
) is export {
  my $slug = Blogin::Slug::slugify($title);
  my $dir  = $section.chars ?? $content.add($section) !! $content;
  my $file = $dir.add("{ $date }-{ $slug }.md");

  die "post already exists: $file (use --force)" if $file.e && !$force;

  $dir.mkdir;
  $file.spurt(post-stub($title, $date));

  $file;
}

sub scaffold-files(Str $framework, Str $date) {
  my %files =
    'blogin.json'                          => blogin-json($framework),
    "content/posts/{ $date }-hello-world.md" => starter-post($date),
    'layouts/base.haml'                    => base-haml($framework),
    'layouts/show.haml'                    => show-haml(),
    'layouts/index.haml'                   => index-haml(),
    'layouts/_entry.haml'                  => entry-haml(),
    'layouts/_header.haml'                 => header-haml(),
    'layouts/_nav.haml'                    => nav-haml(),
    'layouts/_nav-item.haml'               => nav-item-haml(),
    'layouts/_sidebar.haml'                => sidebar-haml(),
    'layouts/_footer.haml'                 => footer-haml(),
    'layouts/_search.haml'                 => search-haml(),
    ;

  %files{'static/style.css'} = style-css() if $framework eq 'none';

  %files;
}

our sub init(
  IO()  $dir,
  Str  :$framework = 'none',
  Bool :$force = False,
  Str  :$date = Date.today.Str,
  --> IO::Path
) is export {
  die "unknown framework '$framework' (known: { @KNOWN-FRAMEWORKS.join(', ') })"
    unless $framework eq any(@KNOWN-FRAMEWORKS);

  if $dir.d && $dir.dir.elems && !$force {
    my @existing = $dir.dir.map(*.basename).sort;
    die "target '$dir' is not empty ({ @existing.join(', ') }); pass --force to overwrite";
  }

  for scaffold-files($framework, $date).kv -> $rel, $content {
    my $path = $dir.add($rel);
    $path.parent.mkdir;
    $path.spurt($content);
  }

  $dir;
}
