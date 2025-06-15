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
  fetch(gh_api_artifacts, { method: "GET", headers: headers })
    .then(response => {
      if (response.status !== 200) {
        return
      }

      response.json().then(data => {
        // Extract version and Git hash from the artifact name.
        // Examples of valid artifact names:
        //
        //   dosbox-staging-linux-x86_64-0.82.0-alpha-7342e
        //   dosbox-staging-macOS-universal-0.82.0-alpha-7342e
        //   dosbox-staging-windows-x64-0.82.0-alpha-7342e
        //
        let platform_re = "[\\w-]*"
        let version_re  = "(\\d+\.\\d+\.\\d+)"
        let hash_re     = "([\\w-\.]+)"

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
  console.log(get_build_link_tr_el(os_name));
  get_build_link_tr_el(os_name).innerHTML = '<span class="error">' + msg1 + '</span>'
  get_build_version_el(os_name).innerHTML = '<span class="error">' + msg2 + '</span>'
  get_build_date_el(os_name).innerHTML    = '<span class="error">' + msg3 + '</span>'
}

// Fetch build status using GitHub API and update HTML
function set_ci_status(workflow_file, os_name, description) {

  // GitHub has strict rate limits for anonymous users: 60 requests per hour.
  // We are requesting only one page, with a limit of 1, with the filter query
  // params.
  let page = 1
  let per_page = 1
  let gh_api_url = "https://api.github.com/repos/dosbox-staging/dosbox-staging/"

  let filter_branch = "main"
  let filter_event  = "push"
  let filter_status = "success"

  const queryParams = new URLSearchParams()
  queryParams.set("page",     page)
  queryParams.set("per_page", per_page)
  queryParams.set("branch",   filter_branch)
  queryParams.set("event",    filter_event)
  queryParams.set("status",   filter_status)

  let gh_api_workflows = gh_api_url + "actions/workflows/" + workflow_file +
                         "/runs?" + queryParams.toString()

  fetch(gh_api_workflows, { method: "GET", headers: headers })
    .then(response => {
      // Handle HTTP error
      if (response.status !== 200) {
        console.warn("Looks like there was a problem." +
                     "Status Code: " + response.status)

        handle_error('Error accessing GitHub API',
                     'Please try again later',
                     'Status: ' + response.status, os_name)
        return
      }

      response.json().then(data => {
        // console.log(data.workflow_runs)
        const status = data.workflow_runs.length && data.workflow_runs[0]

        // If result not found, query the next page
        if (status == undefined) {
            const error_message = `No builds found for ${workflow_file}`
            console.warn(error_message)
            handle_error(error_message, os_name)
            return
        }

        // Update HTML elements
        let build_link = document.createElement("a")
        build_link.textContent = description
        build_link.setAttribute("href", status.html_url)

        let build_link_tr_el = get_build_link_tr_el(os_name)
        build_link_tr_el.innerHTML = ""
        build_link_tr_el.appendChild(build_link)

        let build_date = new Date(status.updated_at)
        get_build_date_el(os_name).textContent = build_date.toUTCString()

        set_build_version(status.artifacts_url, os_name)
      })
    })
    .catch(err => {
      console.warn("Fetch error", err)
    })
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

    The development builds are hosted on GitHub; you need to have a GitHub
    account to download them. If you're not logged in to GitHub, you will see
    the build artifacts but clicking on their names won't initiate the
    download.


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
guide](windows.md#windows-defender) to learn how to deal with this.


### macOS

Please download the universal binary package named
`dosbox-staging-macOS-universal-*`.

macOS development builds are not notarized (but the stable releases are);
Apple Gatekeeper will try to prevent the program from running. See [this
guide](macos.md#apple-gatekeeper) to learn how to deal with this.


### Linux

We provide statically linked x86_64 Linux packages that only depend on C/C++,
ALSA, and OpenGL system libraries.


## How to upgrade your configuration

Testing new features might require a manual reset of the configuration
file.

Start by backing up your existing primary config. These are the standard
non-portable mode locations for each platform:

<div class="compact" markdown>

| <!-- --> | <!-- -->
|----------|----------
| **Windows**  | `C:\Users\%USERNAME%\AppData\Local\DOSBox\dosbox-staging.conf`
| **macOS**    | `~/Library/Preferences/DOSBox/dosbox-staging.conf`
| **Linux**    | `~/.config/dosbox/dosbox-staging.conf`

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
the release notes carefully---everything you need to know about upgrading your
settings is described there.


### The correct way

The 100% correct (but more cumbersome) way is to let DOSBox Staging write the
new default primary config on the first launch, then reapply your old settings
manually. This is very simple: after backing up your existing primary
config, delete it, then start the new version.

For portable installations, put an _empty_ `dosbox-staging.conf` file in the
installation folder to enable portable mode (otherwise DOSBox Staging would
create the new default primary config in the standard non-portable location).

