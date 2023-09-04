# DOSBox Staging website & documentation

## Overview

The website and documentation at the
[https://dosbox-staging.github.io/][website] project website are generated
from the [MkDocs][mkdocs] sources in the [`website`](/website) directory of
this repository. We use a customised version of the [Material for
MkDocs]([mkdocs-material]) theme.

Both the website and documentation sources live in this single MkDocs source tree.

If you're unfamiliar with MkDocs, here are a few pointers to get you started:

- [Getting started with MkDocs](https://www.mkdocs.org/getting-started/)
- [MkDocs User Guide](https://www.mkdocs.org/user-guide/)
- [Material for MkDocs — Reference](https://squidfunk.github.io/mkdocs-material/reference/)

> **Warning** Always modify the MkDocs sources to make changes to the website
> or documentation—*do not* attempt to push any manual changes to our
> organisation-level GitHub Pages repo directly at
> [https://github.com/dosbox-staging/dosbox-staging.github.io/][dosbox-github-pages].
> If you do so, your manual changes will be overwritten by the next proper
> website deployment action.


## Branching strategy

The _current website and documentation_ is always generated from the last stable
release branch (e.g., `release/0.80.x`) to https://dosbox-staging.github.io/.

The _work-in-progress documentation_ for the _next major release_ is deployed from
the `main` branch to https://dosbox-staging.github.io/preview/dev/.

Other work-in-progress documentation revisions might be published under the
`preview` path prefix as needed.


## Submitting changes

### Run the Markdown linter

We use the [Markdown lint tool](https://github.com/markdownlint/markdownlint)
(`mdl` command) to help ensure the consistency of our Markdown files. Once
you're done with your proposed changes, don't just raise a PR yet—you must
run the linter tool first and fix all the warnings it raises.

If you have Ruby already, you can install the tool with `gem install mdl`.
Please refer to the project's documentation for [further installation
instructions](https://github.com/markdownlint/markdownlint).

You can run the linter by executing `../scripts/verify-markdown.sh` from the
[`website`](/website) directory. Please fix all the issues the linter
complains about; you won't be able to merge your PR in the presence of
warnings.


### Raise a PR

Once the linter is happy, it's time to raise your PR. Separate commits may
help in some cases, but don't go overboard with them (we're not going to
"bisect" the documentation, after all). Prefix your documentation related
commits with `docs:` and website related commits with `website:` , e.g.:

```
docs: Fix Sound Blaster Pro 1 default settings
website: Update feature highlights on the front page
```

Please familiarise yourself with [our PR contribution
guidelines](https://github.com/dosbox-staging/dosbox-staging/blob/main/CONTRIBUTING.md#submitting-patches--pull-requests)
if you haven't done so already.


There are two common scenarios when making website/documentation changes:

#### Making fixes or changes to the _current_ website/documentation

- Raise a PR against the lastest _release branch_ (e.g. `releases/0.80.x`).

- Usually, you also want to forward-port the changes to the work-in-progress
  documentation of the next release, so raise a second PR against `main`
  with the same changes.

#### Making changes to the website/documentation for the _next_ major release _only_

- Raise a PR against `main` only.


## Deploying the website & documentation

The website and documentation are deployed via the  [Deploy
website][deploy-website] GitHub Action. This will generate the website from
the MkDocs sources and push the generated content into our [organisation-level
GitHub Pages repo][dosbox-github-pages]. The changes should automatically
appear on the [https://dosbox-staging.github.io/][website] project website
after a few minutes (it might take a bit longer when GitHub is overloaded).

When deploying manually, just accept the defaults when deploying the _current_
website. If you want to deploy the _work-in-progress_ version off `main`,
use `main` as the source branch and enter `dev` for the path prefix.


## Previewing documentation changes locally

### Prerequisites

> **Note**
> If you're comfortable with Linux, using WSL with a Ubuntu guest on Windows
> is highly recommended. We might provide detailed Windows-specific
> instructions later for less technical users if there's a need.

First of all, you need a recent version of
[Python](https://www.python.org/) available on your machine. You should be
able to upgrade to the latest Python with your operating systems's package
manager.

You'll also need the [pip](https://pypi.org/project/pip/) Python package
management tool to install MkDocs and its dependencies. Please refer to
[pip installation instructions](https://pip.pypa.io/en/stable/installation/)
for details.

Our [Deploy website][deploy-website] GitHub Action that builds and deploys the
documentation uses the [Ubuntu
22.04](https://github.com/actions/runner-images/blob/main/images/linux/Ubuntu2204-Readme.md)
image, which comes with Python 3.10.6 and pip3 22.0.2, so these are the
recommended minimum versions.

Once that's done, you can install MkDocs and the required dependencies with
the following command:

``` shell
pip install -r contrib/documentation/mkdocs-package-requirements.txt
```

### Generating the documentation

To simply generate the documentation (without live preview), execute the
`mkdocs build` command. The output will be written to `site` subdirectory
under the [`website`](/website) directory.


### Live preview

To use the convenient live preview feature of MkDocs, run `mkdocs serve`
from the [`website`](/website) folder of the checked-out repo. You can view the
generated documentation at <http://127.0.0.1:8000/dosbox-staging.github.io/>
in your browser when this command is running.

Whenever you make changes to the Markdown files, the website will be
automatically regenerated, and the page in your browser will be refreshed.

> **Warning**
> Sometimes, the local web server gets stuck when you click on a link that
> takes you to a different page; in this case, restart the `mkdocs serve`
> command. A restart might also be needed after more extensive changes (e.g.,
> renaming files, changing the directory structure, etc.)


## General documenetation writing guidelines

### Content 

- The target audience for our documentation is regular computer-savvy
  users—*not* power users, and *definitely not* developers!

- We don't assume any familiarity with MS-DOS or having used computers in the
  80s/90s. A person born after 2000 with no prior experience with
  DOS gaming or PC building should be able to use DOSBox effectively based
  solely on our instructions.

### Style

- We generally recommend following Google's
  [developer documentation style guide](https://developers.google.com/style).
  At the very least, familiarise yourself with the [key points](https://developers.google.com/style/highlights);
  it will greatly improve your style and consistency.

- We use International English as opposed to American English in our
  documentation. This means English as used in the British Commonwealth
  countries, so the United Kingdom, Australia, New Zealand, and Canada
  (among many other English-speaking countries outside of the United States
  of America). In practice, this mostly comes down to using British spelling
  instead of American (e.g., *colour* instead of *color*, *centre* instead
  of *center*, *favourite* instead of *favorite*, *analyse* instead of
  *analyze*, etc.) When in doubt, default to British spelling.

- Many of the maintainers are not native speakers, but the bare minimum
  standard for contributing to the documentation is being able to write
  grammatically correct, clear English prose. Please use one of the many free
  online spell and style checkers—fixing grammar, punctuation, and wording
  issues during the PR process wastes everybody's valuable time.

- Great care has been taken to ensure the consistency of the documentation
  both at the generated content and the underlying Markdown level.
  Therefore, please do not invent your own way of doing things; look for an
  existing example of what you're trying to achieve instead and copy that.
  This is not to discourage adding new constructs or styles when they are
  genuinely needed, but please don't reinvent the wheel for no good reason.

- No one is born a great
  writer! Just as you would not publish your very first working draft of a
  piece of code without cleaning it up first (at least we *really* hope you
  wouldn't! 😎), very few people are able to churn out perfect prose without
  iteratively revising it a few times. Even the greatest literary authors
  need a team of editors, or self-edit and refine their work themselves over long
  periods of time!


### Process

- We treat documentation with the same level of rigor and care as our
  production code, so all contributions must go through the same peer-review
  process. Don't be discouraged if the maintainers request changes or
  refinements—it's all in the interest of ensuring the overall high quality of
  the project, which most definitely includes the documentation! 😎

- If these requirements are too much or too strict for you, that's fine! You
  can still contribute by raising support tickets about missing things that
  are not covered in the existing documentation but should be, or issues in
  the existing documentation that need fixing.


[mkdocs]: https://www.mkdocs.org/
[mkdocs-material]: https://squidfunk.github.io/mkdocs-material/

[website]: https://dosbox-staging.github.io/
[dosbox-github-pages]: https://github.com/dosbox-staging/dosbox-staging.github.io/
[deploy-website]: https://github.com/dosbox-staging/dosbox-staging/actions/workflows/deploy-website.yml
[main-branch]: https://github.com/dosbox-staging/dosbox-staging/tree/main

