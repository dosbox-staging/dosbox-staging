"""
MkDocs hook: adjust navigation and redirects for offline documentation
builds.

Problem
-------

When OFFLINE=true, the 'exclude' plugin (inside the offline group block
in mkdocs.yml) removes all releases/* pages and dummy section-index
pages from the build. However, several other parts of the config/content
still reference those now-missing files:

  1. The 'nav' section lists ~25 releases/*.md entries and dummy index
     pages (getting-started/index.md, manual/index.md) used for section
     redirects.

  2. The 'redirects' plugin has ~20 redirect_maps entries whose targets
     are releases/* or index.md pages (also excluded).

  3. Markdown content in some pages contains relative links to release
     pages (e.g., [Windows releases](../releases/windows.md)).

MkDocs warns loudly about every single one of these dangling references.

Additionally, Python's requests library uses certifi's CA bundle rather
than the OS trust store. VPNs that perform SSL inspection inject a
self-signed root CA that the OS trusts but certifi does not, causing
HTTPS downloads by the privacy plugin to fail with
CERTIFICATE_VERIFY_FAILED. We patch requests to use the system CA bundle
instead, keeping verification intact.

Solution
--------

'on_config' (runs before nav resolution and redirect processing):

  1. Replaces the entire "Releases" nav subtree with a single external
     link to the live website. Offline users get a clickable link to the
     full release information online, and MkDocs has nothing to resolve.

  2. Strips dummy section-index pages from the nav. These only exist to
     support redirects in the online build; in offline mode the
     redirects are cleared and the pages are excluded.

  3. Clears all redirect_maps. Every current redirect targets either a
     releases/* page or an index.md page -- all excluded in offline
     builds. Redirects are also meaningless offline (no web server to
     issue 301s; the generated <meta http-equiv="refresh"> HTML pages
     would never be navigated to because the source URLs don't exist
     either).

  4. Uses the system CA certificate bundle instead of certifi so that
     the privacy plugin can download assets even behind VPNs that
     perform SSL inspection. Falls back to certifi if no system bundle
     is found.

'on_page_markdown' (runs for each page before MkDocs validates links):

  5. Rewrites relative markdown links that target excluded pages
     (releases/* and index.md) into absolute URLs pointing to the live
     website. For example:

       [Windows releases](../releases/windows.md)

     becomes:

       [Windows releases](https://www.dosbox-staging.org/releases/windows/)
"""

import logging
import os
import re
import ssl

import requests.adapters

log = logging.getLogger("mkdocs.hooks.offline")

WEBSITE_URL = "https://www.dosbox-staging.org"

# Dummy section-index pages that only exist for online redirects. These
# are excluded from offline builds (via the exclude plugin in
# mkdocs.yml) and must also be stripped from the nav tree.
DUMMY_INDEX_PAGES = {
    "getting-started/index.md",
    "manual/index.md",
}


def on_config(config):
    """ Only act when building offline documentation (OFFLINE=true).
    The offline_mode extra is set in mkdocs.yml via:
      extra:
        offline_mode: !ENV [OFFLINE, false]
    This is the same flag that main.py's figure() macro checks.
    """
    if not config["extra"].get("offline_mode", False):
        return

    _use_system_ssl_certs()

    # --- Nav: replace "Releases" subtree with an external link --------
    #
    # The nav is a list of dicts. The Releases entry looks like:
    #   {"Releases": [{"Windows": "releases/windows.md"}, ...]}
    #
    # We replace it with an external link in the same format MkDocs uses
    # for the existing Wiki entry (see mkdocs.yml):

    #   {"Releases": "https://www.dosbox-staging.org/releases/"}
    #
    # This way the nav sidebar shows "Releases" as a clickable link to
    # the live website instead of a section with broken child pages.
    #
    for i, item in enumerate(config["nav"]):
        if isinstance(item, dict) and "Releases" in item:
            config["nav"][i] = {"Releases": f"{WEBSITE_URL}/releases/"}
            log.debug("Replaced 'Releases' nav section with external link")
            break

    # --- Nav: strip dummy index pages ---------------------------------
    #
    # Dummy section-index pages (e.g., getting-started/index.md) serve
    # only as redirect sources in online builds. In offline mode they
    # are excluded by the exclude plugin but still referenced in the
    # nav, causing warnings. Remove them so the nav is clean.
    #
    config["nav"] = _strip_dummy_pages_from_nav(config["nav"])
    log.debug("Stripped dummy index pages from nav")

    # --- Redirects: clear all redirect_maps ---------------------------
    #
    # The redirects plugin is loaded outside the offline group block, so
    # it is active in both online and offline builds. Its redirect_maps
    # all target pages that are excluded in offline mode (releases/* or
    # index.md pages). Clearing them prevents "Redirect target does not
    # exist!" warnings.
    #
    # If someone adds non-releases redirects in the future, this will
    # need a filter instead of a blanket clear.
    #
    try:
        redirects = config["plugins"]["redirects"]
        redirects.config["redirect_maps"] = {}
        log.debug("Cleared redirect_maps for offline build")
    except KeyError:
        pass  # redirects plugin not loaded; nothing to do


# pylint: disable=unused-argument
def on_page_markdown(markdown, page, config, files):
    """Rewrite relative links to excluded pages into absolute website
    URLs.

    Some pages contain relative markdown links to excluded pages like:

        [Windows releases](../releases/windows.md)
        [SmartScreen](../releases/windows.md#windows-defender)
        [Feature highlights](../index.md)
        [Getting Started](../getting-started/index.md)

    In offline builds, those targets are excluded by the exclude plugin,
    so MkDocs warns about broken links. This handler rewrites them to
    point to the live website instead.
    """
    if not config["extra"].get("offline_mode", False):
        return markdown

    # Pattern: match standard markdown links [text](path) We only care
    # about relative links (starting with ../) whose resolved path lands
    # on an excluded page: releases/*, or any index.md (top-level or
    # section-level like getting-started/index.md).
    #
    # Capture groups:
    #   1: link text    e.g. "Windows releases"
    #   2: full path    e.g. "../releases/windows.md#windows-defender"
    #   3: normalised   e.g. "releases/windows.md#windows-defender"
    return re.sub(
        r"\[([^\]]*)\]\(((?:\.\./)+(releases/[^)]+|(?:[\w-]+/)*index\.md))\)",
        _rewrite_excluded_link,
        markdown,
    )


def _rewrite_excluded_link(match):
    """Convert a relative .md link into an absolute website URL.

    Handles the .md → directory URL conversion that MkDocs uses with
    use_directory_urls=true (the default):
      releases/windows.md              → /releases/windows/
      releases/windows.md#anchor       → /releases/windows/#anchor
      index.md                         → /
      getting-started/index.md         → /getting-started/
    """
    text = match.group(1)

    # Group 3 is the path without the ../ prefixes (from the inner
    # capture group in the regex). Examples:
    #   releases/windows.md#windows-defender
    #   getting-started/index.md
    #   index.md
    path = match.group(3)

    # Split off any #anchor fragment
    anchor = ""
    if "#" in path:
        path, anchor = path.split("#", 1)
        anchor = "#" + anchor

    # Convert .md file path to a URL path. index.md files map to their
    # parent directory (MkDocs use_directory_urls convention):
    #   index.md                  -> "" (site root)
    #   getting-started/index.md  -> "getting-started"
    #   releases/foo.md           -> "releases/foo"
    url_path = path.removesuffix(".md")
    if url_path == "index":
        url_path = ""
    elif url_path.endswith("/index"):
        url_path = url_path[:-len("/index")]

    return f"[{text}]({WEBSITE_URL}/{url_path}/{anchor})"


def _strip_dummy_pages_from_nav(nav):
    """Remove dummy redirect pages from the nav tree at any depth."""
    result = []
    for item in nav:
        if isinstance(item, str) and item in DUMMY_INDEX_PAGES:
            continue
        if isinstance(item, dict):
            result.append({
                k: _strip_dummy_pages_from_nav(v) if isinstance(v, list) else v
                for k, v in item.items()
            })
        else:
            result.append(item)
    return result


def _use_system_ssl_certs():
    """Use the system CA bundle instead of certifi for asset downloads.

    Python's requests library uses certifi's CA bundle, not the OS trust
    store. VPNs that perform SSL inspection inject a self-signed root CA
    that the OS trusts but certifi does not, causing every HTTPS
    download to fail with CERTIFICATE_VERIFY_FAILED.

    We find the system CA bundle and patch requests to use it, keeping
    SSL verification intact. If no system bundle is found, requests
    falls back to certifi unchanged.

    If the system bundle still lacks the required CA (e.g., a VPN root
    CA not present in /etc/ssl/cert.pem), set the SSL_CERT_FILE
    environment variable to the path of a bundle that includes it.
    """

    candidates = [
        os.environ.get("SSL_CERT_FILE"),
        os.environ.get("REQUESTS_CA_BUNDLE"),
        ssl.get_default_verify_paths().cafile,
        ssl.get_default_verify_paths().openssl_cafile,
        "/etc/ssl/cert.pem",                      # macOS
        "/etc/ssl/certs/ca-certificates.crt",      # Debian / Ubuntu
        "/etc/pki/tls/certs/ca-bundle.crt",        # RHEL / CentOS
    ]

    ca_bundle = next((p for p in candidates if p and os.path.isfile(p)), None)

    if ca_bundle:
        _original_send = requests.adapters.HTTPAdapter.send

        def _send_with_system_certs(self, request,
                                    _orig=_original_send,
                                    _ca=ca_bundle, **kwargs):
            if kwargs.get("verify") is True:
                kwargs["verify"] = _ca
            return _orig(self, request, **kwargs)

        requests.adapters.HTTPAdapter.send = _send_with_system_certs
        log.debug("Using system CA bundle: %s", ca_bundle)
    else:
        log.warning("No system CA bundle found; using certifi defaults")
