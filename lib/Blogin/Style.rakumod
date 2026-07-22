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

.blogin-theme-toggle {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  background: transparent;
  border: 0;
  color: inherit;
  cursor: pointer;
  padding: 0.25rem 0.5rem;
  line-height: 0;
}

.blogin-theme-toggle svg {
  width: 1.25rem;
  height: 1.25rem;
}

.blogin-theme-toggle .icon-moon {
  fill: currentColor;
}

.blogin-theme-toggle .icon-sun {
  display: none;
  fill: none;
  stroke: currentColor;
  stroke-width: 2;
  stroke-linecap: round;
}

[data-theme="dark"] .blogin-theme-toggle .icon-moon {
  display: none;
}

[data-theme="dark"] .blogin-theme-toggle .icon-sun {
  display: inline;
}

[data-theme="dark"] pre {
  background: #161b22;
}

[data-theme="dark"] .hl-keyword { color: #ff7b72; }
[data-theme="dark"] .hl-string { color: #a5d6ff; }
[data-theme="dark"] .hl-number { color: #79c0ff; }
[data-theme="dark"] .hl-comment { color: #8b949e; }

[data-theme="dark"] .anchor {
  color: #6e7681;
}

[data-theme="dark"] dl {
  border-color: #30363d;
  background: #161b22;
}

.navbar-tools {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 0.5rem;
  width: 100%;
  margin-top: 0.5rem;
}

.navbar-search {
  width: 100%;
}

@media (min-width: 992px) {
  .navbar-tools {
    flex-direction: row;
    align-items: center;
    width: auto;
    margin-top: 0;
    margin-left: auto;
  }

  .navbar-search {
    width: 24rem;
    max-width: 90vw;
  }

  .navbar-toggle-slot {
    order: 2;
  }
}
CSS

our sub content-css(--> Str) is export {
  CONTENT-CSS;
}
