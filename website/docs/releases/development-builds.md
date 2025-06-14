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
  set_ci_status("windows.yml", "windows", "Windows ¹")
  set_ci_status("macos.yml",   "macos",   "macOS ²")
  set_ci_status("linux.yml",   "linux",   "Linux ³")
})

</script>


!!! warning

    These are unstable development snapshots intended for testing and
    showcasing new features; if you want to download a stable build, head on
    to the [Windows](windows.md), [macOS](macos.md), or [Linux](linux.md)
    download pages.

    Build artifacts are hosted on GitHub; you need to be logged in to GitHub
    to download them.


<div class="compact">
<table>
  <tr>
    <th style="width: 240px">Download link</th>
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

¹ Windows builds include x86_64 installer and portable ZIP packages, and the
experimental ARM64 portable ZIP package.

² macOS builds include Intel, Apple silicon, and universal binary package.

³ We provide a statically linked x86_64 Linux package (only depends
on C/C++, ALSA, and OpenGL system libraries).


## Installation notes

### Windows

Windows executables in snapshot packages are not signed, therefore Windows 10
or later might prevent the program from starting.

See [this guide](windows.md#windows-defender) to learn how to
deal with this.


### macOS

macOS development builds are not notarized (but the public releases are);
Apple Gatekeeper will try to prevent the program from running.

See [this guide](macos.md#apple-gatekeeper) to learn how to deal with
this.


### Linux

The tarball contains a dynamically-linked x86-64 build; you'll need additional
dependencies installed via your package manager.

#### Fedora

    sudo dnf install SDL2 SDL2_net opusfile

#### Debian, Ubuntu

    sudo apt install libsdl2-2.0-0 libsdl2-net-2.0-0 libopusfile0

#### Arch, Manjaro

    sudo pacman -S sdl2 sdl2_net opusfile


### Upgrading your primary configuration

Testing new features might require a manual reset of the configuration
file.

Since config settings might get renamed, altered, or deprecated between
builds, it's best to let DOSBox Staging write the new default primary config
on the first launch, then reapply your old settings manually.

Start by backing up your existing primary config. This is where to find
it on each platform:

<div class="compact" markdown>

| <!-- --> | <!-- -->
|----------|----------
| **Windows**  | `C:\Users\%USERNAME%\AppData\Local\DOSBox\dosbox-staging.conf`
| **macOS**    | `~/Library/Preferences/DOSBox/dosbox-staging.conf`
| **Linux**    | `~/.config/dosbox/dosbox-staging.conf`

</div>

You can also execute DOSBox Staging with the `--printconf` option to have the
location of the primary config printed to your console.

After backing up the existing primary config, simply start the new version---a
new `dosbox-staging.conf` will be written containing the new defaults and
updated setting descriptions.

!!! note "Portable mode notes"

    In portable mode, `dosbox-staging.conf` resides in the same folder as your
    DOSBox Staging executable. The migration steps for portable mode users are
    as follows:

      - Unpack the new version into a _new_ folder (this is important).
      - Create a new _empty_ `dosbox-staging.conf` file in the new folder to
        enable portable mode.
      - Launch the new version.

    DOSBox Staging will write the new defaults to the empty
    `dosbox-staging.conf` file. After this, you carry over your settings from
    the old primary config manually.

### After upgrading

Look for deprecation warnings in the logs (in yellow or orange colours) and
update your configs accordingly.


