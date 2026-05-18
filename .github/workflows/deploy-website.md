# Website deploy workflows

The following workflows are used to deploy the website and preview websites:

| File                                    | Role
| ---                                     | ---
| `deploy-website.yml`                    | Reusable workflow; does the actual build and publih
| `deploy-website-preview-main.yml`       | Auto-deploys merges to `main` under the `preview/main/` prefix on the website
| `deploy-website-preview-pr.yml`         | Auto-deploys PRs under the `preview/pr-<N>/` prefix on the website
| `deploy-website-preview-pr-cleanup.yml` | Removes `preview/pr-<N>/` from the website on PR close

GitHub Actions does not recurse into subdirectories under
`.github/workflows/`, so the files share a prefix instead of living in a
folder.

## Preview website URLs

- `https://www.dosbox-staging.org/preview/main/` --- preview website of the
    current work-in-progress alpha version (the next release)

- `https://www.dosbox-staging.org/preview/pr-<N>/` --- preview websites for
    PRs containing documentation changes

The PR-deploy workflow also posts a sticky comment on the PR with the preview
URL; the cleanup workflow updates that same comment when the preview is
removed.

## Reusable deploy workflow

The `deploy-website.yml` builds the MkDocs site from a given branch and pushes
the output to the separate `dosbox-staging/dosbox-staging.github.io` GitHub
Pages repo. It has two triggers:

- `workflow_dispatch` --- manual entry point for release deploys to root
- `workflow_call` --- invoked by the three wrappers below

The workflow has two inputs:

- `source_branch` (required) --- branch of `dosbox-staging` to build from
- `destination_prefix` (optional):
  - If empty, deploys to the root of the Pages repo (live website at
    https://www.dosbos.staging.org)
  - If non-empty, deploys with the `preview/<prefix>/` to the preview website
    available at https://www.dosbos.staging.org/preview/<prefix>/

When deploying to root, the deletion step explicitly preserves `./preview/*`,
`./static`, `./tools`, and older version dirs like `./0.82`. That keeps
release deploys from clobbering preview content.

## Auto-deploys

| Workflow                                | Auto trigger                                                              | Deploys to
| ---                                     | ---                                                                       | ---
| `deploy-website-preview-main.yml`       | push to `main`, paths: `website/**`                                       | `preview/main/`
| `deploy-website-preview-pr.yml`         | PR opened / synchronize / reopened, **base: `main`**, paths: `website/**` | `preview/pr-<N>/`
| `deploy-website-preview-pr-cleanup.yml` | PR closed, **base: `main`**                                               | (removes `preview/pr-<N>/`)

Auto-deploys are deliberately scoped to `main` and PRs targeting `main`. Deploys
to the live website root (and any work on `release/*` branches) remain a manual
operation via `deploy-website.yml`'s `workflow_dispatch`.

Each wrapper has its own `workflow_dispatch` button, so you can manually rerun
any of them if needed.

## Fork PRs

PRs from forks are skipped --- they don't have access to the deploy token. A
maintainer who wants to preview a fork PR can push the same branch to the main
repo and trigger `deploy-website-preview-pr.yml` manually with the target PR
number.

## Concurrency

All four workflows use `cancel-in-progress: true` and the following
concurrency groups:

- `deploy-website-preview-main` --- main auto-deploy and its manual trigger.
- `deploy-website-preview-pr-<N>` --- shared by the PR deploy and the PR
  cleanup. A `closed` event therefore cancels any in-flight deploy for the
  same PR.

Because cancel-in-progress is not strictly synchronous (a cancelled step may
still complete), the deploy and cleanup workflows both wrap their final `git
push` in a bounded `pull --rebase` & retry loop. If a cancelled sibling lands
its push first, the next one rebases and tries again.

## Retrying a failed run

Use GitHub's built-in "Re-run failed jobs" button on the workflow run page.
The re-run replays the original event payload (same PR, same SHA), so it
just retries against the current state of the source branch.

