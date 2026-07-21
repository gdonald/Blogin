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

.post-nav {
  display: flex;
  justify-content: space-between;
  gap: 1rem;
  margin-top: 2rem;
}

.post-nav .next {
  margin-left: auto;
  text-align: right;
}

.blogin-btn {
  display: inline-block;
  padding: 0.5rem 1rem;
  background: #0d6efd;
  color: #fff;
  border-radius: 0.375rem;
  text-decoration: none;
}

.blogin-btn:hover {
  background: #0b5ed7;
  color: #fff;
}

dl {
  margin: 1.5rem 0;
  padding: 1rem 1.25rem;
  border: 1px solid #e5e7eb;
  border-radius: 0.5rem;
  background: #f9fafb;
}

dl dt {
  font-weight: 600;
}

dl dd {
  margin: 0.25rem 0 1rem;
}

dl dd:last-child {
  margin-bottom: 0;
}
CSS

our sub content-css(--> Str) is export {
  CONTENT-CSS;
}
