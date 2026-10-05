---
hide:
  - footer
---

# Development builds

<style>
span.error {
  font-weight: bold;
  font-size: 95%;
  color: red;
}
</style>

<script>

// For local testing only: uncomment and replace API_TOKEN with a valid GitHub
// API token. This is to bypass the low hourly rate limits for unauthenticated
// API access during testing (only 60 requests per hour).
//
// !!! IMPORTANT -- *NEVER* check in your API token into the repo !!!
//
let headers = {
//  "Authorization": "bearer API_TOKEN"
}

function get_build_link_tr_el(os_name) {
  return document.getElementById(os_name + "-build-link")
}
function get_build_version_el(os_name) {
  return document.getElementById(os_name + "-build-version")
}
function get_build_date_el(os_name) {
  return document.getElementById(os_name + "-build-date")
}

function set_build_version(gh_api_artifacts, os_name) {
  const url = new URL(gh_api_artifacts)
  url.searchParams.set("per_page", "100")
  url.searchParams.set("direction", "desc")
  url.searchParams.set("_", Date.now())

  fetch(url, { method: "GET", headers: headers })
    .then(response => {
      if (response.status !== 200) {
        return
      }

      response.json().then(data => {
        // Extract version and Git hash from the artifact name.
        // Examples of valid artifact names:
        //
        //   dosbox-staging-linux-x86_64-0.84.0-RC1-c9524
        //   dosbox-staging-macOS-universal-0.84.0-RC1-c9524
        //   dosbox-staging-windows-x64-0.84.0-alpha-7342e
        //
        let platform_re = "[\\w-]*"
        let version_re  = "(\\d+\\.\\d+\\.\\d+)"
        let hash_re     = "((?:alpha|RC\\d*|rc\\d*)-[\\w]{5})"
        let re = `dosbox-staging-${platform_re}-${version_re}-${hash_re}`
        let release = data.artifacts.find(a => a.name.match(re))

        if (release === undefined) {
          return
        }

        let match = release.name.match(re)
        let version = match[1]
        let hash    = match[2]

        get_build_version_el(os_name).textContent = `${version}-${hash}`
      })
    })
    .catch(err => {
      console.log("Fetch error", err)
    })
}

function handle_error(msg1, msg2, msg3, os_name) {
  console.log(get_build_link_tr_el(os_name))

  get_build_link_tr_el(os_name).innerHTML = '<span class="error">' + msg1 + '</span>'
  get_build_version_el(os_name).innerHTML = '<span class="error">' + msg2 + '</span>'
  get_build_date_el(os_name).innerHTML    = '<span class="error">' + msg3 + '</span>'
}

async function get_workflow_runs(workflow_file, since, until) {
  let gh_api_url = "https://api.github.com/repos/dosbox-staging/dosbox-staging/"

  let queryParams = new URLSearchParams()
  queryParams.set("per_page", "100")
  queryParams.set("page", "1")
  queryParams.set("branch", "main")
  queryParams.set("event", "push")
  queryParams.set("status", "success")

  // Restrict the search to the requested time range.
  queryParams.set("created", `${since.toISOString()}..${until.toISOString()}`)

  let url = gh_api_url + "actions/workflows/" + workflow_file +
            "/runs?" + queryParams.toString()

  let response = await fetch(url, { method: "GET", headers: headers })
  if (response.status !== 200) {
    throw new Error("GitHub API returned status " + response.status)
  }

  return await response.json()
}

// The GitHub REST API doesn't provide a 'sort' parameter when querying the
// list of CI builds. Therefore, if there are more results than the requested
// page size, we cannot rely on the order of the returned results to identify
// the latest build.
//
// Fortunately, it is possible to request the data only for a given date
// range, so we'll use that to ensure we get a complete result set:
//
// - Start with all successful runs in the last 7 days.
// - If 0 results: expand the window, up to 90 days.
// - If 1–99 results: we're safe; explicitly select the newest run from the
//   complete result set.
// - If 100+ results: the result is truncated, so binary-search for a smaller
//   time window until we have fewer than 100.
//
// Once we have a complete result set, explicitly sort by created_at, with run
// id as the deterministic tie-breaker.
//
// The GraphQL API could simply return the latest result sorted by date, but
// that requires authentication. We don't want to share an API token publicly
// in the JavaScript if it can be avoided.
//
async function find_latest_build(workflow_file) {
  const now = new Date()
  const max_days = 90
  const max_seconds = max_days * 24 * 60 * 60

  // Start by looking at the last seven days.
  let high_seconds = 7 * 24 * 60 * 60

  let since = new Date(now.getTime() - high_seconds * 1000)
  let data = await get_workflow_runs(workflow_file, since, now)

  // Expand the search window until we find at least one build.
  // Doubling the window keeps the number of API requests low.
  while (data.total_count === 0 && high_seconds < max_seconds) {
    high_seconds = Math.min(high_seconds * 2, max_seconds)
    since = new Date(now.getTime() - high_seconds * 1000)
    data = await get_workflow_runs(workflow_file, since, now)
  }

  if (data.total_count === 0) {
    return undefined
  }

  // Fewer than 100 results means the complete result set fits in
  // the first page, so we can select the newest run locally.
  if (data.total_count < 100) {
    return select_latest_run(data.workflow_runs)
  }

  // Narrow the search window until fewer than 100 results are found.
  // This avoids relying on the API's unspecified result ordering.
  let low_seconds = 1
  since = new Date(now.getTime() - low_seconds * 1000)
  let low_data = await get_workflow_runs(workflow_file, since, now)

  if (low_data.total_count >= 100) {
    throw new Error(
      `More than 100 successful ${workflow_file} runs found within one second`
    )
  }

  while (high_seconds - low_seconds > 1) {
    const mid_seconds = Math.floor((low_seconds + high_seconds) / 2)

    since = new Date(now.getTime() - mid_seconds * 1000)
    const mid_data = await get_workflow_runs(workflow_file, since, now)

    if (mid_data.total_count < 100) {
      low_seconds = mid_seconds
      low_data = mid_data
    } else {
      high_seconds = mid_seconds
    }
  }

  if (low_data.total_count === 0) {
    throw new Error(`Unable to find a successful ${workflow_file} run`)
  }

  return select_latest_run(low_data.workflow_runs)
}

function select_latest_run(runs) {
  if (runs.length === 0) {
    return undefined
  }

  // Do not depend on GitHub's response ordering.
  // created_at determines which build is newer; id provides a deterministic
  // tie-breaker if two runs have the same creation timestamp.
  runs.sort((a, b) => {
    const date_diff =
      new Date(b.created_at) - new Date(a.created_at)

    return date_diff || b.id - a.id
  })

  return runs[0]
}

// Fetch build status using GitHub API and update HTML.
async function set_ci_status(workflow_file, os_name, description) {
  try {
    const status =
      await find_latest_build(workflow_file)

    if (status === undefined) {
      const error_message = `No builds found for ${workflow_file}`
      console.warn(error_message)
      handle_error(
        error_message,
        "Please try again later",
        "",
        os_name
      )
      return
    }

    // Update HTML elements.
    let build_link = document.createElement("a")
    build_link.textContent = description
    build_link.setAttribute("href", status.html_url)

    let build_link_tr_el = get_build_link_tr_el(os_name)
    build_link_tr_el.innerHTML = ""
    build_link_tr_el.appendChild(build_link)

    let build_date = new Date(status.created_at)
    let date_string_utc = new Intl.DateTimeFormat('en-GB', {
      timeZone: 'UTC',
      timeZoneName: 'short',
      year: 'numeric',
      month: 'short',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    }).format(build_date)

    get_build_date_el(os_name).textContent = date_string_utc

    set_build_version(status.artifacts_url, os_name)
  } catch (err) {
    console.warn("Fetch error", err)

    handle_error(
      "Error accessing GitHub API",
      "Please try again later",
      err.message,
      os_name
    )
  }
}

document.addEventListener("DOMContentLoaded", () => {
  set_ci_status("windows.yml", "windows", "Windows")
  set_ci_status("macos.yml",   "macos",   "macOS")
  set_ci_status("linux.yml",   "linux",   "Linux")
})

</script>

!!! warning

    These are unstable development snapshots intended for testing and
    showcasing new features. Things might blow up! :bomb: :fire: :skull:

    Less adventurous users wanting to download the latest stable build should
    head on to the [Windows](windows.md), [macOS](macos.md), or
    [Linux](linux.md) download pages.


!!! info

    The **development builds** are hosted on GitHub; you'll need a GitHub
    account to download them. If you're not logged in to GitHub, you will see
    the build artifacts, but clicking on their names won't initiate the
    download.

    We release a new dev snapshot build whenever a PR is merged. Make sure to
    check out the automaticaly generated **release notes HTML** included with
    the snapshot builds. This lists all the changes since the last stable
    release, with links to the individual PRs on GitHub.


<div class="compact">
<table>
  <tr>
    <th style="width: 240px">Download page</th>
    <th style="width: 250px">Build version</th>
    <th style="width: 300px">Date</th>
  </tr>
  <tr>
    <td id="windows-build-link">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
    <td id="windows-build-version">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
    <td id="windows-build-date">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
  </tr>
  <tr>
    <td id="macos-build-link">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
    <td id="macos-build-version">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
    <td id="macos-build-date">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
  </tr>
  <tr>
    <td id="linux-build-link">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
    <td id="linux-build-version">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
    <td id="linux-build-date">
      <img style="margin:auto;margin-left:0.1em;" src="../images/dots.svg">
    </td>
  </tr>
</table>
</div>


## Installation notes

### Windows

Windows builds include x86_64 installer (artifact name ending with `-setup`)
and portable ZIP packages.

The Windows executables are not signed, therefore Windows 10 or later might
prevent the program from starting. See [this
guide](../0.83/manual/using-dosbox-staging/starting.md#windows-defender)
to learn how to deal with this.


### macOS

Please download the universal binary package named
`dosbox-staging-macOS-universal-*`.

macOS development builds are not notarized (but the stable releases are);
Apple Gatekeeper will try to prevent the program from running. See [this
guide](macos.md#apple-gatekeeper) to learn how to deal with this.


### Linux

We provide statically linked x86_64 Linux packages that only depend on C/C++,
ALSA, and OpenGL system libraries.


## Upgrading your configuration

Testing new features might require a manual reset of the configuration
file.

Start by backing up your existing primary config. These are the standard
non-portable mode locations for each platform:

<div class="compact" markdown>

| <!-- --> | <!-- -->
|----------|----------
| **Windows**  | `C:\Users\%USERNAME%\AppData\Local\DOSBox\dosbox-staging.conf`
| **macOS**    | `/Users/<USERNAME>/Library/Preferences/DOSBox/dosbox-staging.conf`
| **Linux**    | `$HOME/.config/dosbox/dosbox-staging.conf`

</div>

In portable mode, `dosbox-staging.conf` resides in the same folder as your
DOSBox Staging executable.

Alternatively, run DOSBox Staging with the `--printconf` option, which will
print the location of the primary config to your console.


### The easy way

Once you've backed up your primary config, start the new version, then run
`config -wcd` to update the primary config. That's it!

This method will work 90% of the time, but certain setting changes cannot be
automatically migrated. Look for deprecation warnings in the logs (in yellow
or orange colour), then compare your current and old configs and update your
settings accordingly.

Check out the new config descriptions for guidance, and also make sure to read
the release notes carefully --- everything you need to know about upgrading your
settings is described there.


### The correct way

The 100% correct (but more cumbersome) way is to let DOSBox Staging write the
new default primary config on the first launch, then reapply your old settings
manually. This is very simple: after backing up your existing primary
config, delete it, then start the new version.

For portable installations, put an _empty_ `dosbox-staging.conf` file in the
installation folder to enable portable mode (otherwise DOSBox Staging would
create the new default primary config in the standard non-portable location).

