---
title: Authorization
slug: authorization
order: 15
description: A policy controls which actions an admin may perform, which records they can see, and which menu entries and buttons render. The default policy allows...
toc: true
---
A policy controls which actions an admin may perform, which records they can see,
and which menu entries and buttons render. The default policy allows everything,
so authorization is opt-in.

## The policy interface

A policy answers two questions:

```raku
method allows(Str:D $action, :$admin, :$resource, :$record --> Bool) { ... }
method scope($relation, :$admin, :$resource) { ... }
```

`allows` authorizes an action. The action is one of `index`, `show`, `create`,
`update`, `destroy`, or the name of a custom member or collection action.
`$admin` is the `current-admin`, `$resource` is the registered resource, and
`$record` is set for record-level checks (show, edit, update, destroy, member
actions) and absent otherwise.

`scope` returns a restricted base relation. It is applied to the index listing
and to record lookups, so records outside the scope are neither listed nor
reachable (a lookup returns 404).

Install a policy with:

```raku
MVC::Keayl::Admin.authorize-with(MyPolicy.new);
```

## Enforcement

A forbidden action renders a 403 page. Record-level checks run after the record
is loaded. The base relation is scoped before listing and before lookups.

When the policy forbids an action, its controls are hidden: menu entries (when
`index` is forbidden), the new button (`create`), row show, edit, and delete
actions, custom action buttons, and batch options.

## Writing a policy

Subclass the base policy and override what you need; the base allows everything
and scopes nothing.

```raku
use MVC::Keayl::Admin::Authorization::Policy;

class OwnedRecords is MVC::Keayl::Admin::Authorization::Policy {
  method scope($relation, :$admin, :$resource) {
    $relation.where({ owner_id => $admin.id })
  }

  method allows(Str:D $action, :$admin, :$resource, :$record --> Bool) {
    return True without $record;
    $record.read-attribute('owner_id') == $admin.id
  }
}
```

## Role-based adapter

`MVC::Keayl::Admin::Authorization::Role` authorizes by the admin's role against a
permissions map. A `*` entry permits every action.

```raku
use MVC::Keayl::Admin::Authorization::Role;

MVC::Keayl::Admin.authorize-with(
  MVC::Keayl::Admin::Authorization::Role.new(
    permissions => {
      viewer => <index show>,
      editor => <index show create update>,
      admin  => <*>,
    },
  )
);
```

By default the role is read from `$admin.role`. Pass `role-of` to extract it
differently:

```raku
MVC::Keayl::Admin::Authorization::Role.new(
  permissions => %permissions,
  role-of     => -> $admin { $admin.account.role-name },
);
```
