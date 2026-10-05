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

const gh_api_url = "https://api.github.com/repos/dosbox-staging/dosbox-staging/"

// Our CI publishes each platform's latest successful build from 'main' to a
// rolling prerelease, then moves that release's tag to the commit the build
// came from (see the 'publish_dev_build' jobs in '.github/workflows').
//
// Reading those releases gives us a deterministic answer in a single request
// per platform. Querying the list of CI builds instead would be unreliable:
// that API provides no 'sort' parameter and the order of the returned results
// is unspecified, so different visitors can end up seeing different builds.
//
const dev_builds = [
  {
    os_name:     "windows",
    release_tag: "dev-latest-windows",
    downloads: [
      { label: "Installer",    re: /-setup\.exe$/ },
      { label: "Portable ZIP", re: /\.zip$/ }
    ]
  },
  {
    os_name:     "macos",
    release_tag: "dev-latest-macos",
    downloads: [
      { label: "Disk image",    re: /\.dmg$/ },
      { label: "Release notes", re: /\.html$/ }
    ]
  },
  {
    os_name:     "linux",
    release_tag: "dev-latest-linux",
    downloads: [
      { label: "Tarball", re: /\.tar\.xz$/ }
    ]
  }
]

function get_build_link_tr_el(os_name) {
  return document.getElementById(os_name + "-build-link")
}
function get_build_version_el(os_name) {
  return document.getElementById(os_name + "-build-version")
}
function get_build_date_el(os_name) {
  return document.getElementById(os_name + "-build-date")
}

function handle_error(msg1, msg2, msg3, os_name) {
  get_build_link_tr_el(os_name).innerHTML  = '<span class="error">' + msg1 + '</span>'
  get_build_version_el(os_name).innerHTML  = '<span class="error">' + msg2 + '</span>'
  get_build_date_el(os_name).innerHTML     = '<span class="error">' + msg3 + '</span>'
}

async function fetch_json(url, what) {
  const response = await fetch(url, { method: "GET", headers: headers })

  if (response.status !== 200) {
    throw new Error(`Could not fetch ${what} (status ${response.status})`)
  }

  return await response.json()
}

function set_download_links(downloads, os_name) {
  const el = get_build_link_tr_el(os_name)
  el.innerHTML = ""

  downloads.forEach((download, i) => {
    if (i > 0) {
      el.appendChild(document.createElement("br"))
    }

    const link = document.createElement("a")
    link.textContent = download.label
    link.setAttribute("href", download.asset.browser_download_url)
    el.appendChild(link)
  })
}

function set_build_date(asset, os_name) {
  // 'updated_at' is when the asset was uploaded, so it reflects when the
  // build was published.
  const date_string_utc = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'UTC',
    timeZoneName: 'short',
    year: 'numeric',
    month: 'short',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  }).format(new Date(asset.updated_at))

  get_build_date_el(os_name).textContent = date_string_utc
}

async function set_dev_build(build) {
  try {
    // The cache busting parameter is needed because GitHub serves these
    // responses with an ETag.
    const release = await fetch_json(
      `${gh_api_url}releases/tags/${build.release_tag}?_=${Date.now()}`,
      "the latest development build"
    )

    // Our CI sets the release title to the version of the build whose assets
    // are current, and only once every one of them has finished uploading.
    // Restricting ourselves to that version therefore always yields a single
    // complete build, never a mix of the new and the previous one.
    const version = release.name
    const assets  = release.assets.filter(asset => asset.name.includes(version))

    const downloads = build.downloads
      .map(download => ({
        label: download.label,
        asset: assets.find(asset => download.re.test(asset.name))
      }))
      .filter(download => download.asset !== undefined)

    if (downloads.length === 0) {
      const error_message =
        `No ${version} downloads found in ${build.release_tag}`
      console.warn(error_message)
      handle_error(error_message, "Please try again later", "", build.os_name)
      return
    }

    set_download_links(downloads, build.os_name)
    set_build_date(downloads[0].asset, build.os_name)

    get_build_version_el(build.os_name).textContent = version

  } catch (err) {
    console.warn("Fetch error", err)

    handle_error(
      "Error accessing GitHub API",
      "Please try again later",
      err.message,
      build.os_name
    )
  }
}

document.addEventListener("DOMContentLoaded", () => {
  dev_builds.forEach(build => set_dev_build(build))
})
</script>


!!! warning

    These are unstable development snapshots intended for testing and
    showcasing new features. Things might blow up! :bomb: :fire: :skull:

    Less adventurous users wanting to download the latest stable build should
    head on to the [Windows](windows.md), [macOS](macos.md), or
    [Linux](linux.md) download pages.


!!! info

    The **development builds** are hosted on GitHub; no GitHub account is
    needed to download them.

    We release a new dev snapshot build whenever a PR is merged. Make sure to
    check out the automatically generated **release notes HTML** included with
    the snapshot builds (macOS users get it as a separate download). This
    lists all the changes since the last stable release, with links to the
    individual PRs on GitHub.


<div class="compact">
<table>
  <tr>
    <th style="width: 240px">Download</th>
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

