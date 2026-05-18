# Website deploy workflows

Five files cover everything that publishes the MkDocs site to
`dosbox-staging.github.io`:

| File | Role |
| --- | --- |
| `deploy-website.yml` | Reusable workflow. Does the actual build + push. |
| `deploy-website-preview-main.yml` | Auto-deploy `main` → `preview/main/`. |
| `deploy-website-preview-pr.yml` | Auto-deploy PRs → `preview/pr-<N>/`. |
| `deploy-website-preview-pr-cleanup.yml` | Remove `preview/pr-<N>/` on PR close. |
| `deploy-website.md` | This file. |

GitHub Actions does not recurse into subdirectories under
`.github/workflows/`, so the files share a prefix instead of living in a
folder.

## The reusable workflow: `deploy-website.yml`

Builds the MkDocs site from a given branch and pushes the output to the
separate `dosbox-staging/dosbox-staging.github.io` Pages repo. Two triggers:

- `workflow_dispatch` — manual entry point for release deploys to root.
- `workflow_call` — invoked by the three wrappers below.

Two inputs:

- `source_branch` (required) — branch of `dosbox-staging` to build from.
- `destination_prefix` (optional):
  - empty → deploys to the root of the Pages repo (production release).
  - non-empty → deploys to `preview/<prefix>/`.

When deploying to root, the deletion step explicitly preserves
`./preview/*`, `./static`, `./tools`, and older version dirs like `./0.82`.
That keeps release deploys from clobbering preview content.

## Auto-deploys

| Workflow | Auto trigger | Manual trigger inputs | Deploys to |
| --- | --- | --- | --- |
| `deploy-website-preview-main.yml` | push to `main`, paths: `website/**` | none | `preview/main/` |
| `deploy-website-preview-pr.yml` | PR opened / synchronize / reopened, paths: `website/**` | `pr_number`, `branch` | `preview/pr-<N>/` |
| `deploy-website-preview-pr-cleanup.yml` | PR closed | `pr_number` | (removes `preview/pr-<N>/`) |

Each wrapper has its own `workflow_dispatch` button (Actions → pick workflow
→ Run workflow) so you can manually rerun any of them if needed.

## Live URLs

- `https://www.dosbox-staging.org/preview/main/`
- `https://www.dosbox-staging.org/preview/pr-<N>/`

The PR-deploy workflow also posts a sticky comment on the PR with the
preview URL; the cleanup workflow updates that same comment when the
preview is removed.

## Fork PRs

PRs from forks are skipped — they don't have access to the deploy token. A
maintainer who wants to preview a fork PR can push the same branch to the
main repo and trigger `deploy-website-preview-pr.yml` manually with the
target PR number.

## Concurrency

All four workflows use `cancel-in-progress: true`. Concurrency groups:

- `deploy-website-preview-main` — main auto-deploy and its manual trigger.
- `deploy-website-preview-pr-<N>` — shared by the PR deploy and the PR
  cleanup. A `closed` event therefore cancels any in-flight deploy for the
  same PR.

Because cancel-in-progress is not strictly synchronous (a cancelled step
may still complete), the deploy and cleanup workflows both wrap their final
`git push` in a bounded `pull --rebase` + retry loop. If a cancelled
sibling lands its push first, the next one rebases and tries again.

## Retrying a failed run

Use GitHub's built-in "Re-run failed jobs" button on the workflow run page.
The re-run replays the original event payload (same PR, same SHA), so it
just retries against the current state of the source branch.

## Adding another auto-deploy

Copy `deploy-website-preview-main.yml`, adjust the trigger, and set the two
inputs (`source_branch`, `destination_prefix`) to whatever you want. No
other changes needed — the reusable workflow handles the rest.
