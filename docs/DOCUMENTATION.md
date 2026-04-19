# DOSBox Staging website & documentation

## Overview

The website and documentation at
[https://www.dosbox-staging.org/][website] are generated
from the [MkDocs][mkdocs] sources in the [`website`](/website) directory of
this repository. We use a customised version of the [Material for
MkDocs]([mkdocs-material]) theme.

The [Deploy website][deploy-website] GitHub workflow publishes the
generated website to our organisation-level GitHub Pages repository at
[https://github.com/dosbox-staging/dosbox-staging.github.io/][dosbox-github-pages].

The website and documentation sources live in a single MkDocs source tree,
including a minimal amount of small image files. Large binary files (e.g.,
images, audio and video files) are checked in directly to the
[static](https://github.com/dosbox-staging/dosbox-staging.github.io/tree/master/static)
directory of the [GitHub Pages repo][dosbox-github-pages]. This is necessary
to keep the size of the [main DOSBox Staging repo][dosbox-staging] to a
minimum. The [Deploy website workflow][deploy-website] leaves the `static`
directory alone; its contents are managed entirely manually.

If you're unfamiliar with MkDocs and MkDocs Material, here are a few pointers
to get you started:

- [Getting started with MkDocs](https://www.mkdocs.org/getting-started/)
- [MkDocs User Guide](https://www.mkdocs.org/user-guide/)
- [Material for MkDocs — Reference](https://squidfunk.github.io/mkdocs-material/reference/)

> [!WARNING]
> Always modify the MkDocs sources to make changes to the website
> or documentation --- *do not* attempt to push any manual changes to our
> organisation-level GitHub Pages repo directly at
> [https://github.com/dosbox-staging/dosbox-staging.github.io/][dosbox-github-pages].
> If you do so, your manual changes will be overwritten by the next run of the
> deploy website workflow!
>
> As noted above, the only exception to this is storing large binary
> files in the
> [static](https://github.com/dosbox-staging/dosbox-staging.github.io/tree/master/static)
> directory.


## Branching strategy

The **current website and documentation** are always generated from the last
stable release branch (e.g., `release/0.82.x`) to
https://www.dosbox-staging.org/.

The **work-in-progress documentation** for the next release (in other words,
the current alpha version) is deployed from the `main` branch to
https://www.dosbox-staging.org/preview/dev/.

Other work-in-progress documentation revisions might be published under the
`preview` path prefix as needed.


## Submitting changes

### Proofread your changes


Use the [preview feature](#previewing-website--documentation-changes-locally)
of MkDocs to proofread your changes from start to finish. Make sure the text
flows well, conforms to our
[guidelines](#general-documenetation-writing-guidelines), and **run the text
through a spellchecker** and fix all mistakes. This ensures the professional,
high-quality results we're striving for. Also, relying on the reviewer to
catch and flag typos and grammatical errors is a waste of everybody's time.


### Run the Markdown linter

We use the [Markdown lint tool](https://github.com/markdownlint/markdownlint)
(`mdl` command) to help ensure the consistency of our Markdown files. Once
you're done with your proposed changes, don't just raise a PR yet --- you must
run the linter tool first and fix all the warnings it raises.

If you have Ruby already, you can install the tool with `gem install mdl`.
Please refer to the project's documentation for [further installation
instructions](https://github.com/markdownlint/markdownlint).

You can run the linter by executing `../scripts/linting/verify-markdown.sh`
from the [`website`](/website) directory. Please fix all the issues the linter
complains about; you won't be able to merge your PR as long as there are
warnings.


### Raise a PR

Once you, the spellchecker, and the linter are all happy, it's time to raise
your PR. Separate commits may help in some cases, but don't go overboard with
them (we're not going to "bisect" the documentation, after all). Use your
judgement. Prefix your documentation-related commits with `docs:` and
website-related commits with `website:`. For example:

```
docs: Fix Sound Blaster Pro 1 default settings
website: Update feature highlights on the front page
```

Formatting-only changes that don't add or fix content should be done in their
own commits as they tend to be very noisy:

```
docs: Fix wrapping and add extra new lines between sections
```

Please familiarise yourself with [our PR contribution
guidelines](CONTRIBUTING.md#submitting-patches--pull-requests) if you haven't
done so already.

There are two common scenarios when making website/documentation changes:

#### Making changes to the _current_ website and documentation of the _last stable release_

- Raise a PR against the latest **release branch** (e.g. `releases/0.82.x`).

- Usually, you'll also want to forward-port the changes to the
  documentation of the current alpha version (the next release we're working
  on), so raise a **second PR** against `main` with the same changes.
  Sometimes, you'll need to adjust the second PR if the website/documentation
  of the alpha version has diverged.

#### Making changes to the website and documentation of the _next release_ (current work-in-progress alpha release)

- Raise a PR against `main` only.


## Deploying the website & documentation

The website and documentation are deployed via the [Deploy
website][deploy-website] GitHub workflow. This will generate the website from
the MkDocs sources and push the generated content into our [organisation-level
GitHub Pages repo][dosbox-github-pages]. The changes should automatically
appear on the [https://www.dosbox-staging.org/][website] project website after
a few minutes (it might take a bit longer when GitHub is overloaded).

### Deploying **the current website and documentation** for the latest stable release

Run the [Deploy website][deploy-website] action, then accept the defaults in
the appearing dialog. The changes will be available on
https://www.dosbox-staging.org/ in a few minutes.

### Deploying the **work-in-progress website and documentation** for the current alpha version off `main`

Run the [Deploy website][deploy-website] action, enter `main` as the source
branch in the appearing dialog and `dev` for the path prefix. The deploy will
be available under https://www.dosbox-staging.org/preview/dev/ in a few
minutes. Of course, you can use other "preview" tags than `dev`, too, but use
these sparingly.

> [!WARNING] Be very careful when deploying a preview website off `main`!
> Triple-check that you have entered the `dev` path prefix --- if you leave it
> empty, you'll deploy the current alpha website/documentation to our live
> website!
>
> **Always double-check** the success of your deploy on the live website after
> a few minutes to confirm you haven't screwed up the deploy!


## Previewing website & documentation changes locally

### Prerequisites

First of all, you'll need a recent version of [Python
3](https://www.python.org/) available on your machine. You should be able to
upgrade to the latest Python with your operating system's package manager.

You'll also need the [pip](https://pypi.org/project/pip/) Python package
management tool to install MkDocs and its dependencies. Please refer to
[pip installation instructions](https://pip.pypa.io/en/stable/installation/)
for details.

The easiest way for **Windows users** is to use the [official Python
installer](https://www.python.org/downloads/), which will install both Python 3
and pip. Make sure to check **"Add Python to PATH"** during installation.

After this, you can install MkDocs and the required dependencies with the
following command:

``` shell
pip install -r extras/documentation/mkdocs-package-requirements.txt
```

### Generating the website & documentation

To generate the documentation (without live preview), run the `mkdocs build`
command from the [`website`](/website) directory. The output will be written
to `website/site`.


### Live preview

To use the convenient live preview feature of MkDocs, run `mkdocs serve` from
the [`website`](/website) directory. You can view the generated documentation
at <http://127.0.0.1:8000/> in your browser when this command is running.

Whenever you make changes to the source files under `docs`, the website will
be automatically regenerated, and the page in your browser will be refreshed.

The live preview doesn't work when editing the front page
(`overrides/home.html`), which makes sense as it's not located under the
`docs` directory. You'll need to restart the `mkdocs server` command to view
your changes.

> [!WARNING] On some systems, the local web server might "get stuck" when you
> click on a link that takes you to a different page. If this happens, restart
> the `mkdocs serve` command. A restart might also be needed after more
> extensive changes (e.g., renaming or moving files, changing the directory
> structure, etc.)
>
> On other systems, saving the Markdown files might not always (or ever)
> trigger live reloading the page in the browser. Try restarting your browser,
> and if that doesn't help, try forcing live reloading with `mkdocs serve
> --livereload`.


### Changing the stylesheet

The `docs/stylesheets/extra.css` CSS file is generated from the
`docs/extra-scss/extra.scss` [SASS](https://sass-lang.com/) source file.

To live preview editing the SASS source file, you'll need to run the below
command from the `website` directory _in addition to_ running `mkdocs server`
(so you'll need to open two command prompts):

```
sass --watch extra-scss/extra.scss:docs/stylesheets/extra.css
```

To generate the CSS file just once without live preview, drop the `--watch`
option.

Make sure to **always** regenerate the CSS file after changing the SASS source,
then check in both the `.scss` source file and the generated `.css` files.


## General documentation writing guidelines

### Content

- The target audience for our documentation is regular computer-savvy
  users --- *not* power users, and *definitely not* developers!

- We must not assume any familiarity with MS-DOS or having used computers in
  the 1980s/1990s. A person born after 2000 with no prior experience with DOS
  gaming or PC building should be able to use DOSBox effectively based solely
  on our instructions.


### Style

- We generally recommend following Google's
  [developer documentation style guide](https://developers.google.com/style).
  At the very least, familiarise yourself with the
  [key points](https://developers.google.com/style/highlights);
  it will greatly improve your style and consistency.

- We use International English as opposed to American English in our
  documentation. This means English as used in the British Commonwealth
  countries, such as the United Kingdom, Australia, New Zealand, and Canada
  (among many other English-speaking countries outside of the United States
  of America). In practice, this mostly comes down to using British spelling
  instead of American (e.g., *colour* instead of *color*, *centre* instead
  of *center*, *favourite* instead of *favorite*, *analyse* instead of
  *analyze*, etc.) When in doubt, default to British spelling.

- Not all our maintainers are native speakers, but the bare minimum
  standard for contributing to the documentation is the ability to write
  grammatically correct, clear English prose. Please use one of the many
  free online spell and style checkers --- fixing grammar, punctuation, and
  wording issues during the PR process wastes everybody's time.

- Great care has been taken to ensure the consistency of the documentation
  both at the generated content and the underlying Markdown level.
  Therefore, please do not invent your own way of doing things; look for an
  existing example of what you're trying to achieve and copy that. This is
  not to discourage people from adding new constructs or styles when they
  are genuinely needed, but please don't reinvent the wheel for no good
  reason. If you're intending to do anything radical, consult the core team
  first. If you suspect your change would lead to questions, ask the
  maintainers first.

- No one is born a great writer! Just as you would not publish your very first
  working draft of a piece of code without cleaning it up first (at least we
  *really* hope you wouldn't! 😎), very few people are able to churn out
  perfect prose without iteratively revising it a few times. Even the greatest
  literary authors need a team of editors, or self-edit and refine their work
  themselves over long periods of time!

- LLMs are great at writing documentation or turning rough drafts into
  well-flowing, grammatically correct English prose. If you're short on time,
  or you're not feeling confident in your writing abilities, by all means use
  LLM help! Of course, ensuring the correctness of the final output is still
  your responsibility, but we don't particularly care how you reached your
  destination.


### Process

- We treat documentation with the same level of rigour and care as our
  production code, so all contributions must pass the same peer-review
  process. Don't be discouraged if the maintainers request changes or
  refinements --- it's all in the interest of ensuring high
  quality outcomes.

- If these requirements are too much or too strict for you, that's fine! You
  can still contribute by raising support tickets for issues in
  the manual that need fixing.


[mkdocs]: https://www.mkdocs.org/
[mkdocs-material]: https://squidfunk.github.io/mkdocs-material/

[website]: https://www.dosbox-staging.org/
[dosbox-staging]: https://github.com/dosbox-staging/dosbox-staging/
[dosbox-github-pages]: https://github.com/dosbox-staging/dosbox-staging.github.io/
[deploy-website]: https://github.com/dosbox-staging/dosbox-staging/actions/workflows/deploy-website.yml
[main-branch]: https://github.com/dosbox-staging/dosbox-staging/tree/main


## Deploy website action

The [Deploy website][deploy-website] GitHub workflow needs to push the
generated files to our [organisation-level GitHub Pages
repo](dosbox-github-pages). Such cross-repository access is a bit tricky; the
deploy action achieves it via a personal access token. We use tokens with an
expiry for security reasons, so at some point the **Commit & publish new
release** step of the deploy action will start to fail. You'll need to
generate a new token when this happens as described below.

### Generate a new classic personal access token

1. When logged in to GitHub as the administrator of the DOSBox Staging
   organisation, click on your user profile picture in the top right corner,
   then on the **Settings** menu (cog icon).

2. Go to the **Developer settings** page (bottom-most item in the left menu
   pane).

3. Go to the **Personalised access tokens / Tokens (classic)** page.

4. Click on the **Generate new token** button, then select **Generate new
   token (classic)**.

5. Give your new token a name and an expiry date (say a year from now), then
   enable all repo privileges by checking the **repo** item.

6. Press **Generate token** at the bottom of the page, then copy the generated
   token (you won't be able to retrieve it later; you'll need to generate a
   new one if you lose it).

### Set up the secret access token

1. Go to the [Settings page](https://github.com/dosbox-staging/dosbox-staging/settings) of the
   [dosbox-staging][dosbox-staging]  repo.

2. Open the **Secrets and variables** menu in the left pane, then select
   **Actions**.

3. Edit the `ACCESS_TOKEN_REPO` item in **Repository secrets** (or add it if
   it doesn't exist), paste in the new token, then click the **Update secret**
   button.

After these steps, the deploy website action should run without errors.

