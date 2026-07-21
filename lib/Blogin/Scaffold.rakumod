use v6.d;

use Blogin::Slug;

unit module Blogin::Scaffold;

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

sub base-haml(--> Str) {
  q:to/HAML/;
  !!! 5
  %html{lang: 'en'}
    %head
      %meta{charset: 'utf-8'}
      %meta{name: 'viewport', content: 'width=device-width, initial-scale=1'}
      %title= site-title
      != head-meta
      != framework-stylesheet-tag
      %link{rel: 'stylesheet', href: '/blogin.css'}
      %link{rel: 'stylesheet', href: '/style.css'}
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
      != framework-script-tag
  HAML
}

sub show-haml(--> Str) {
  q:to/HAML/;
  %article
    %h1= title
    - if show-dates
      %p.meta= "#{date} · #{reading-time} min read"
    - if has-toc
      %nav.toc
        != toc-html
    != body
  != post-nav-html
  - if has-related
    %nav.related
      %h2 Related posts
      %ul
        != render(:partial<entry>, :collection(related), :as<entry>)
  HAML
}

sub index-haml(--> Str) {
  q:to/HAML/;
  %section.listing
    %h1= section-label
    %ul
      != render(:partial<entry>, :collection(posts), :as<entry>)
    != pagination-html
  HAML
}

sub entry-haml(--> Str) {
  q:to/HAML/;
  %li
    %a{href: "#{$entry<url>}"}= $entry<title>
    - if index-dates
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
  .blogin-search
    %form{'data-blogin-search' => 'true'}
      %input{type: 'search', name: 'q', placeholder: 'Search'}
    %ul{'data-blogin-results' => 'true'}
  %link{rel: 'stylesheet', href: '/search.css'}
  %script{src: '/search.js'}
  HAML
}

sub bootstrap-base-haml(--> Str) {
  q:to/HAML/;
  !!! 5
  %html{lang: 'en'}
    %head
      %meta{charset: 'utf-8'}
      %meta{name: 'viewport', content: 'width=device-width, initial-scale=1'}
      %title= site-title
      != head-meta
      != framework-stylesheet-tag
      %link{rel: 'stylesheet', href: '/blogin.css'}
      %link{rel: 'stylesheet', href: '/style.css'}
    %body.d-flex.flex-column.min-vh-100
      - if has-header
        != debug-open('partial: header')
        != render(:partial<header>, :locals(%( brand => site-title )))
        != debug-close('partial: header')
      .container.my-4.flex-grow-1
        .row.g-4
          %main.col-lg-8
            != debug-open(template-label)
            = yield
            != debug-close(template-label)
          - if has-sidebar
            %aside.col-lg-4
              != render(:partial<sidebar>)
      - if has-footer
        != debug-open('partial: footer')
        != render(:partial<footer>)
        != debug-close('partial: footer')
      != framework-script-tag
  HAML
}

sub bootstrap-header-haml(--> Str) {
  q:to/HAML/;
  %nav.navbar.navbar-expand-lg.navbar-dark.bg-dark
    .container
      %a.navbar-brand{href: '/'}= $brand
      %button.navbar-toggler{type: 'button', 'data-bs-toggle' => 'collapse', 'data-bs-target' => '#topnav', 'aria-controls' => 'topnav', 'aria-expanded' => 'false', 'aria-label' => 'Toggle navigation'}
        %span.navbar-toggler-icon
      #topnav.collapse.navbar-collapse
        != render(:partial<nav>)
  HAML
}

sub bootstrap-nav-haml(--> Str) {
  q:to/HAML/;
  %ul.navbar-nav.me-auto.mb-2.mb-lg-0
    != render(:partial<nav-item>, :collection(nav-nodes), :as<node>)
  HAML
}

sub bootstrap-nav-item-haml(--> Str) {
  q:to/HAML/;
  %li.nav-item
    - if nav-current($node)
      %a.nav-link.active{href: "#{$node.url}"}= $node.label
    - else
      %a.nav-link{href: "#{$node.url}"}= $node.label
  HAML
}

sub bootstrap-sidebar-haml(--> Str) {
  q:to/HAML/;
  %section
    != render(:partial<search>)
  HAML
}

sub bootstrap-footer-haml(--> Str) {
  q:to/HAML/;
  %footer.border-top.py-3.mt-auto
    .container
      %p.text-body-secondary.mb-0 Built with Blogin.
  HAML
}

sub bootstrap-index-haml(--> Str) {
  q:to/HAML/;
  %section
    %h1.mb-4= section-label
    .list-group
      != render(:partial<entry>, :collection(posts), :as<entry>)
    != pagination-html
  HAML
}

sub bootstrap-entry-haml(--> Str) {
  q:to/HAML/;
  %a.list-group-item.list-group-item-action{href: "#{$entry<url>}"}
    %span= $entry<title>
    - if index-dates
      %span.text-body-secondary.ms-2= $entry<date>
  HAML
}

sub style-css(--> Str) {
  '';
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
    'layouts/base.haml'                    => base-haml(),
    'layouts/show.haml'                    => show-haml(),
    'layouts/index.haml'                   => index-haml(),
    'layouts/_entry.haml'                  => entry-haml(),
    'layouts/_header.haml'                 => header-haml(),
    'layouts/_nav.haml'                    => nav-haml(),
    'layouts/_nav-item.haml'               => nav-item-haml(),
    'layouts/_sidebar.haml'                => sidebar-haml(),
    'layouts/_footer.haml'                 => footer-haml(),
    'layouts/_search.haml'                 => search-haml(),
    'static/style.css'                     => style-css(),
    ;

  if $framework eq 'bootstrap5' {
    %files{'layouts/base.haml'}      = bootstrap-base-haml();
    %files{'layouts/index.haml'}     = bootstrap-index-haml();
    %files{'layouts/_entry.haml'}    = bootstrap-entry-haml();
    %files{'layouts/_header.haml'}   = bootstrap-header-haml();
    %files{'layouts/_nav.haml'}      = bootstrap-nav-haml();
    %files{'layouts/_nav-item.haml'} = bootstrap-nav-item-haml();
    %files{'layouts/_sidebar.haml'}  = bootstrap-sidebar-haml();
    %files{'layouts/_footer.haml'}   = bootstrap-footer-haml();
  }

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
