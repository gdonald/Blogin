---
title: Dogfooding My Own Raku MVC Web Framework
date: 2026-07-11
slug: dogfooding-my-own-raku-mvc-web-framework
description: A few weeks ago I blog'd about shipping behave.dev , the first Raku app I ever put into production. This blog post is an even bigger deal to me personally. The site you are reading right now,...
tags: [admin, mvc, orm, raku]
archives: [2026-07]
---
A few weeks ago [I blog'd](https://gregdonald.com/pages/shipping-my-first-raku-app) about shipping [behave.dev](https://behave.dev/), the first [Raku](https://raku.org) app I ever put into production. This blog post is an even bigger deal to me personally. The site you are reading right now, [gregdonald.com](https://gregdonald.com/), no longer runs on [Ruby on Rails](https://rubyonrails.org). It runs on a new [MVC](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93controller) web framework that I created. The web framework, the ORM, and the admin are all my own Raku distributions, and the only honest way to trust a library you created is to run something you care about on top of it.

I have been writing and deploying web apps for a long time, and for most of the last two decades my usual answer to "what should this run on" has been Rails. Rewriting my own site to run on my own framework is the kind of thing you do once you are ready to live with your own decisions every time a page loads. It's a bit scary.

The stack is three dists I created and maintain.

**[MVC::Keayl](https://github.com/gdonald/MVC-Keayl)**

This is the MVC web framework. It has models, views, and controllers as well as routing and middleware. This site is laid out the way you would expect: `app/controllers`, `app/views`, `app/models`, and a `config/routes.raku` that maps requests to actions like `home#index`, `pages#show`, and `imgs#show`. It's served up via a [Cro](https://cro.services) adapter behind [nginx](https://nginx.org), with a small stack of middleware for static files, request ids, and logging. Views are written in [HAML](https://github.com/gdonald/Template-HAML), compiled once in production but with hot reloading in development.

**[ORM::ActiveRecord](https://github.com/gdonald/ORM-ActiveRecord)**

Models and migrations. Each migration is a file under `db/migrate` named like `20260628000403-create-pages.raku`, and a single command brings a database up to date.

```bash
$ active-record db:migrate
```

The models are plain Raku classes that know how to query, associate, and validate. The blog posts, tags, and images that make up this site are all records the ORM loads and the controllers hand to the views.

**[MVC::Keayl::Admin](https://github.com/gdonald/MVC-Keayl-Admin)**

The admin, mounted at `/admin`. It is how I write and edit these posts, manage tags, and upload images without touching [SQL](https://en.wikipedia.org/wiki/SQL). I love SQL, but I don't want to write boilerplate [CRUD](https://en.wikipedia.org/wiki/Create,_read,_update_and_delete) SQL all the time. Building the public site and the admin on the same ORM means a change to a model shows up in both places at once, which is exactly the feedback loop I wanted.

**Getting the old data across**

The old site was a standard Rails app, and my images lived in [Active Storage](https://guides.rubyonrails.org/active_storage_overview.html). I did not want to lose twenty years of blog posts or re-upload every image by hand, so I wrote a sync script that converts my Rails data and images over to MVC::Keayl and ORM::ActiveRecord format in one go. It pulls the data down, converts it locally, and leaves me with a database and a pile of image files that my new app can read as-is.

Pulling down is two transfers. The script [sshes](https://www.openssh.com) into the old production box, runs my [Postgres](https://www.postgresql.org) backup, and copies the compressed dump back to my machine. Then it [rsyncs](https://rsync.samba.org) the Active Storage directory down so I have every original image byte sitting locally next to the dump.

The conversion all happens on my development machine, where a mistake costs nothing. It drops and recreates a fresh local database, loads the dump into it, and records the content migrations as already applied so they do not try to recreate tables the dump already carries. Then it runs only the migrations Rails never had, which are the Keayl storage tables. With the schema in place, it walks every Active Storage attachment and converts it into the Keayl store, re-uploading the bytes through the disk service so the new site owns them going forward. The posts, tags, and image records came straight across because the schema lines up.

Pushing up is the mirror image. I dump the converted local database, copy it to the new server, and restore it into the production database there. Then I rsync the converted image tree up into `shared/storage`, the same shared path each release symlinks its `storage` directory at. At that point the new app has the data and the files exactly where it expects them.

I did not do this once and hope. I ran the whole pull, convert, and push for weeks before launch. The script rebuilds the local database from scratch on every run and skips any image it has already imported, so it is safe to run over and over, and I did run it many times. Each run was a full rehearsal of launch day. By the time I actually cut over, I was not running anything new. I was running commands I had already run dozens of times and watching them do the same thing they always did. The only difference on launch day was that I pointed [DNS](https://en.wikipedia.org/wiki/Domain_Name_System) at the new box when it was done.

**Deploying it**

The deploy is deliberately boring, as usual. A bare [git](https://git-scm.com) repo on the server, a timestamped release checked out on each deploy, a `current` symlink that flips atomically, and a [systemd](https://systemd.io) user service that runs the app and restarts on failure.

```text
/raku/gregdonald.com
├── repo        # bare git repo
├── releases/   # one timestamped checkout per deploy
├── shared/
│   └── storage # uploaded image bytes, kept across releases
└── current -> releases/20260711214209
```

The main piece that matters for a site with uploads is `shared/storage`. Each release gets its `storage` directory symlinked at that shared path, so the images survive a deploy instead of vanishing with the old release.

**Testing it**

None of this would have felt safe to attempt without tests, and the tests run on more of my own Raku code. I write specs with [BDD::Behave](https://behave.dev/), my behavior-driven testing framework, so a spec reads as nested `describe`, `context`, and `it` blocks that state behavior in plain English.

The specs are layered. Model specs check queries, associations, and validations against a real database. Request specs drive the controllers and assert on the responses they return. Then the browser specs open the site in a real browser and click through it the way an actual visitor would.

That browser layer is two more distributions I created. [WWW::Playwright](https://github.com/gdonald/WWW-Playwright) is a Raku binding to [Playwright](https://playwright.dev), so I can drive a real [Chromium](https://www.chromium.org) browser from Raku instead of shelling out to another language. [BDD::Behave::Playwright](https://github.com/gdonald/BDD-Behave-Playwright) bridges that into behave, so a browser spec reads in the same behave syntax as the rest of the suite, with steps that navigate pages, fill in forms, and assert on what the browser actually rendered. That is how I test the MVC::Keayl::Admin flows from end to end. Log in, create a page, upload an image, then load the public site and confirm the image is really there.

I also keep a migration round-trip spec that drops every table and runs all of the migrations from an empty database. It tells me the schema builds cleanly from nothing, not just on the database I happen to already have. That spec earned its keep on this project, because the entire migration was an exercise in standing up a clean database and filling it.

**Some things that bit me**

The first one was certificates. I'm not some rank amateur who allows downtime when moving DNS or web servers, or to an entirely new MVC web framework, for that matter. I live for the complexity of zero-downtime transitions! So I wanted working [TLS](https://en.wikipedia.org/wiki/Transport_Layer_Security) on the new server before I moved DNS, so I could load the real domain over [HTTPS](https://en.wikipedia.org/wiki/HTTPS) and check it against the new box while the old one was still serving traffic. The usual [Let's Encrypt](https://letsencrypt.org) flow proves you control the domain by answering a challenge over port 80 at that domain, but the domain still pointed at the old server, so that check could only ever reach the old box. The way around it is the *"DNS challenge"*. [Certbot](https://certbot.eff.org) hands you a [TXT record](https://en.wikipedia.org/wiki/TXT_record) value, you add it to your domain's DNS, and it proves control without the domain needing to resolve to the new server at all. Once DNS was re-pointed, I switched renewal back to the standard nginx challenge so it renews on its own from then on. Handy.

The second one I brought on myself. I created my own MVC web framework, and I still got caught off-guard by my own MVC web framework. MVC::Keayl and ORM::ActiveRecord each decide their environment from environment variables, and they do not read the same ones. Setting the Keayl variable alone left the ORM defaulting to development, so the production app happily tried to connect to the wrong database config and returned a [500](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/500) with no useful log, since logging is minimal in production. Setting `RAKU_ENV` satisfies both, and now the unit sets exactly that.

The third one is the storage symlink issue from above. I shipped the first release without it, so every image on the site served a placeholder while the real bytes sat untouched in `shared/storage`. These are all the kind of bug you only find by running the real thing, which is the entire argument for dogfooding.

Now every request to this site runs my code from the socket up through the router, the ORM, and the template. When something breaks, I own the whole path, and I can fix it in the same repositories I ship and share.
