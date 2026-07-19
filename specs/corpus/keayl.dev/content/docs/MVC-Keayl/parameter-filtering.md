---
title: Parameter filtering
slug: parameter-filtering
order: 20
description: MVC::Keayl::ParameterFilter redacts sensitive parameters so they are safe to log. It replaces the value of any matching key with [FILTERED], recursing...
toc: true
---
`MVC::Keayl::ParameterFilter` redacts sensitive parameters so they are safe to
log. It replaces the value of any matching key with `[FILTERED]`, recursing
through nested hashes and arrays.

```perl6
MVC::Keayl::ParameterFilter.new.filter(%( password => 'secret', name => 'Ada' ));
# { password => '[FILTERED]', name => 'Ada' }
```

A key matches when it contains one of the filter strings (case-insensitive) or
matches a filter regex. The defaults cover the usual sensitive names: `passw`,
`secret`, `token`, `_key`, `crypt`, `salt`, `certificate`, `otp`, and `ssn`. A
matched key has its whole value redacted, even a nested hash.

## Configuring the list

`also` adds names to the defaults; `filters` replaces them. A filter may be a
string or a regex:

```perl6
MVC::Keayl::ParameterFilter.new(also => ['pin']);
MVC::Keayl::ParameterFilter.new(filters => [/^ card /, 'cvv']);
```

## On a controller

A controller exposes `filtered-params`, the request parameters with the default
and configured filters applied, for logging. Add controller-level names with
`filter-parameters`:

```perl6
class PaymentsController is MVC::Keayl::Controller { }
PaymentsController.filter-parameters('pin', 'cvv');

# in an action
self.filtered-params;   # safe to write to a log
```

It also has an `is filter-parameters` trait form for the class header:

```perl6
class PaymentsController is MVC::Keayl::Controller is filter-parameters('pin', 'cvv') { }
```

Filtering copies the parameters, so the controller's `params` are untouched.
