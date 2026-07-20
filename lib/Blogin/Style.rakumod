use v6.d;

unit module Blogin::Style;

my constant CONTENT-CSS = q:to/CSS/;
pre {
  background: #f6f8fa;
  padding: 1rem;
  overflow-x: auto;
  border-radius: 6px;
}

code {
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}

.hl-keyword { color: #d73a49; }
.hl-string { color: #032f62; }
.hl-number { color: #005cc5; }
.hl-comment { color: #6a737d; font-style: italic; }

.anchor {
  visibility: hidden;
  margin-left: 0.35rem;
  color: #adb5bd;
  text-decoration: none;
}

h1:hover > .anchor,
h2:hover > .anchor,
h3:hover > .anchor,
h4:hover > .anchor,
h5:hover > .anchor,
h6:hover > .anchor {
  visibility: visible;
}
CSS

our sub content-css(--> Str) is export {
  CONTENT-CSS;
}
