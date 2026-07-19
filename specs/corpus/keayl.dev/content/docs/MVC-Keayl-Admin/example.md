---
title: Worked example
slug: example
order: 20
description: This wires a small blog into a grouped, filtered, authorized admin: authors and their posts, with scopes, a custom action, a role policy, and a dashboard...
toc: true
---
This wires a small blog into a grouped, filtered, authorized admin: authors and
their posts, with scopes, a custom action, a role policy, and a dashboard panel.

## The models

Two ORM models, an author with many posts:

```raku
class Author is Model {
  has-many 'posts';
}

class Post is Model {
  belongs-to 'author';

  self.validate: 'title', { :presence };
}
```

## Registering the resources

Both resources go under a `Content` menu group. Posts get columns, a filter, a
scope, a form, and a custom member action; authors get a nested form for their
posts.

```raku
use MVC::Keayl::Admin;

MVC::Keayl::Admin.configure(site-title => 'Blog Admin', mount-path => '/admin');

MVC::Keayl::Admin.register(Post, {
  menu(group => 'Content', icon => 'file-text', priority => 1);

  column('title', :sortable);
  column('author', format => 'link-to-show');
  column('published', :sortable);

  filter('title', :as<string>);
  filter('published', :as<boolean>);

  scope('drafts',    -> $relation { $relation.where({ published => False }) });
  scope('published', -> $relation { $relation.where({ published => True  }) }, :default);

  field('title');
  field('body', :as<text>);
  field('published', :as<boolean>);
  field('author-id', :as<select>, collection => { Author.all.all.map({ .id => .read-attribute('name') }) });

  permit('title', 'body', 'published', 'author-id');

  member-action('publish', -> $controller, $record {
    $record.update({ published => True });
    $controller.redirect-to($controller.path-for('post', $record.id));
  }, :confirm('Publish this post?'));
});

MVC::Keayl::Admin.register(Author, {
  menu(group => 'Content', icon => 'people', priority => 2);

  column('name', :sortable);

  field('name');

  nested('posts', :multiple, {
    field('title');
    field('published', :as<boolean>);
  });

  permit('name');
});
```

## Authorizing access

A role policy: editors manage posts but only view authors; admins do everything.

```raku
use MVC::Keayl::Admin::Authorization::Role;

MVC::Keayl::Admin.authorize-with(
  MVC::Keayl::Admin::Authorization::Role.new(
    permissions => {
      editor => <index show create update publish>,
      admin  => <*>,
    },
  )
);
```

Forbidden actions return a 403, and the controls for them (the new button, row
edit and delete, the publish action, batch destroy) are hidden from a role that
lacks them.

## A dashboard panel and ordering

Order the menu groups and add a dashboard panel listing recent posts:

```raku
MVC::Keayl::Admin.menu-group-order('Content');

MVC::Keayl::Admin.dashboard-block(title => 'Recent posts', {
  '<ul class="list-unstyled mb-0">'
    ~ Post.all.order('created_at DESC').limit(5).all.map({ '<li>' ~ .read-attribute('title') ~ '</li>' }).join
    ~ '</ul>'
});
```

## The result

- A `Content` group in the sidebar with Posts and Authors, Posts first.
- A sortable, filterable post index with `drafts` and `published` scope tabs,
  defaulting to published, plus CSV and JSON export of the current view.
- A post form with a body text area and an author select, and an author form
  that edits its posts inline.
- A `Publish` action on the post show page, gated by the policy.
- A dashboard listing the resources and a recent-posts panel.
