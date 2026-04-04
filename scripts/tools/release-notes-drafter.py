#!/usr/bin/env python3

# SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
# SPDX-License-Identifier: MIT

"""
Create release notes draft for DOSBox Staging.

The GitHub access token must be set in the GITHUB_ACCESS_TOKEN env var.

The token must have the following scopes:
  repo, workflow, write:packages, delete:packages

"""

# pylint: disable=invalid-name
# pylint: disable=import-error
# pylint: disable=missing-docstring
# pylint: disable=global-statement

import argparse
import csv
import datetime
import json
import os
import re
import sys

import markdown2
import requests


GITHUB_ORG          = "dosbox-staging"
GITHUB_REPO         = "dosbox-staging"
GITHUB_API_URL      = "https://api.github.com"
GITHUB_ACCESS_TOKEN = ""

RELEASES_BASE_URL = f"{GITHUB_API_URL}/repos/{GITHUB_ORG}/{GITHUB_REPO}/releases"

COMMON_VARS_PATH = os.path.join(os.path.dirname(__file__),
                                "../../.github/actions/set-common-vars/action.yml")

FETCH_PAGE_SIZE = 50

HTTP_TIMEOUT_SEC = 10

# All PRs are placed in a big bucket first, then filtering is performed
# in priority order, from top to bottom:
#
# - If at least one of the PR's labels matches against the filter's
#   labels, the PR belongs to that category and is removed from the
#   bucket (except if "remove" is False; it defaults to True).
#
# - The bucket of remaining items is passed down to the next
#   filter, and this process is repeated until there are no more filters
#   left.
#
# - If there are still some leftover PRs in the bucket, those will
#   belong to the "other" catch-all category.
#
FILTERS = [
    {
        "category": "game-compatibility",
        "title":    "game compatibility improvements",
        "labels":   ["game compatibility"],
        "remove":   False
    }, {
        "category": "graphics",
        "title":    "graphics-related changes",
        "labels":   ["video", "voodoo", "shaders"]
    }, {
        "category": "sound",
        "title":    "sound-related changes",
        "labels":   ["audio", "midi"]
    }, {
        "category": "input-handling",
        "title":    "input-related changes",
        "labels":   ["input handling"]
    }, {
        "category": "dos-integration",
        "title":    "DOS integration related changes",
        "labels":   ["DOS"]
    }, {
        "category": "localisation",
        "title":    "localisation-related changes",
        "labels":   ["localisation", "translation"]
    }, {
        "category": "documentation",
        "title":    "documentation-related changes",
        "labels":   ["documentation"]
    }, {
        "category": "project-maintenance",
        "title":    "project maintenance related changes",
        "labels":   ["ci", "build system", "cleanup", "packaging",
                     "plumbing", "refactoring", "upstream sync"]
    }, {
        "category": "misc-enhancements",
        "title":    "miscellaneous enhancements",
        "labels":   ["enhancement"]
    }, {
        "category": "misc-fixes",
        "title":    "miscellaneous fixes",
        "labels":   ["bug", "regression"]
    }
]


# ANSI colour helpers — automatically disabled when stdout is not a TTY
# (e.g. when output is piped or redirected).
def _ansi(code):
    def apply(text):
        if sys.stdout.isatty():
            return f"\033[{code}m{text}\033[0m"
        return text
    return apply

_BOLD  = _ansi("1")
_GREEN = _ansi("32")
_CYAN  = _ansi("36")
_YELLOW = _ansi("33")
_DIM   = _ansi("2")


class CustomFormatter(argparse.RawDescriptionHelpFormatter):
    """Coloured, grouped help for both the top-level and subcommand parsers."""

    # Top-level: render subcommand list
    def _format_action(self, action):
        if action.nargs == argparse.PARSER:
            rows = [(name, sub.description or "")
                    for name, sub in action.choices.items()]

            lines = []
            last_prefix = None
            col = 30

            for name, help_text in rows:
                prefix = name.split(".")[0]
                if prefix != last_prefix:
                    if lines:
                        lines.append("")
                    last_prefix = prefix

                styled_name = _GREEN(f"{name:<{col}}")
                lines.append(f"  {styled_name}{help_text}")

            lines.append(
                f"\n{_DIM('Run')} "
                f"{_CYAN('./release-notes-drafter.py <command> --help')} "
                f"{_DIM('for command-specific usage.')}"
            )
            return "\n".join(lines) + "\n"

        # Subcommand: colour option names and positional arg names
        result = super()._format_action(action)
        if action.option_strings:
            for opt in action.option_strings:
                result = result.replace(opt, _CYAN(opt), 1)
        elif action.dest != argparse.SUPPRESS:
            result = result.replace(action.dest, _GREEN(action.dest), 1)
        return result

    # Colour section headers (e.g. "positional arguments:")
    def start_section(self, heading):
        super().start_section(_BOLD(_YELLOW(heading)) if heading else heading)

    # Colour the "usage:" prefix
    def _format_usage(self, usage, actions, groups, prefix):
        if prefix is None:
            prefix = _BOLD("usage: ")
        return super()._format_usage(usage, actions, groups, prefix)


def setup_arg_parser(parser):
    fmt = CustomFormatter
    subparsers = parser.add_subparsers(dest="ACTION", required=True,
                                       metavar="<command>")

    # summary.query
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "summary.query",
        description="Fetch merged PRs and write to CSV.",
        formatter_class=fmt,
    )
    p.add_argument("out_pull_requests_csv",
                   help="output CSV file for pull requests")
    p.add_argument("--start_time",
                   help="include PRs after this datetime "
                        "(defaults to RELEASE_START_TIME from set-common-vars/action.yml)")

    # summary.process_markdown
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "summary.process_markdown",
        description="Generate release notes Markdown from CSV.",
        formatter_class=fmt,
    )
    p.add_argument("in_pull_requests_csv", help="input CSV file")
    p.add_argument("out_markdown",         help="output Markdown file")

    # summary.process_html
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "summary.process_html",
        description="Generate release notes HTML from CSV.",
        formatter_class=fmt,
    )
    p.add_argument("in_pull_requests_csv", help="input CSV file")
    p.add_argument("process_version_tag",  help="version tag (e.g. v0.83.0-alpha)")
    p.add_argument("out_html",             help="output HTML file")

    # summary.process_csv
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "summary.process_csv",
        description="Generate categorised CSV from input CSV.",
        formatter_class=fmt,
    )
    p.add_argument("in_pull_requests_csv", help="input CSV file")
    p.add_argument("out_categorised_csv",  help="output categorised CSV file")

    # summary.publish
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "summary.publish",
        description="Publish (create or update) the release notes draft.",
        formatter_class=fmt,
    )
    p.add_argument("in_markdown",          help="input Markdown file")
    p.add_argument("publish_version_tag",  help="version tag (e.g. v0.83.0-alpha)")

    # website.query
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "website.query",
        description="Fetch merged PRs with descriptions and write to JSON.",
        formatter_class=fmt,
    )
    p.add_argument("out_pull_requests_json", help="output JSON file")
    p.add_argument("--start_time",
                   help="include PRs after this datetime "
                        "(defaults to RELEASE_START_TIME from set-common-vars/action.yml)")

    # website.process_markdown
    # -----------------------------------------------------------------
    p = subparsers.add_parser(
        "website.process_markdown",
        description="Generate detailed website release notes from JSON.",
        formatter_class=fmt,
    )
    p.add_argument("in_pull_requests_json", help="input JSON file from website.query")
    p.add_argument("process_version_tag",   help="version tag (e.g. v0.83.0-alpha)")
    p.add_argument("out_markdown",          help="output Markdown file")


def get_default_start_time():
    """Read RELEASE_START_TIME from the CI common vars file."""
    path = os.path.normpath(COMMON_VARS_PATH)
    with open(path, encoding="UTF-8") as f:
        for line in f:
            m = re.search(r'RELEASE_START_TIME=(\S+)', line)
            if m:
                return m.group(1).strip('"\'')
    return None


def create_headers():
    return {"Authorization": f"bearer {GITHUB_ACCESS_TOKEN}"}


def print_api_error(request, response):
    print("GitHub API returned an error.\n\n"
          f"Request:\n\n  {request}\n\n"
          f"Response status: {str(response.status_code)}, body: \n\n  "
          + response.text)


def execute_query(query):
    url = f"{GITHUB_API_URL}/graphql"
    data = json.dumps({"query": query})

    r = requests.post(url, headers=create_headers(), data=data,
                      timeout=HTTP_TIMEOUT_SEC)

    if r.status_code != 200:
        print_api_error(data, r)
        sys.exit(1)

    response = json.loads(r.text)

    if "errors" in response:
        print_api_error(data, r)
        sys.exit(1)

    return response


def make_repository_query(body):
    return """
      {
        repository(owner: \"""" + GITHUB_ORG + """\", name: \"""" + GITHUB_REPO + """\") {
          """ + body + """
        }
      }
    """

def get_latest_public_release():
    query = make_repository_query("""
          latestRelease {
            name
            createdAt
            publishedAt
          }
    """)
    response = execute_query(query)
    return response["data"]["repository"]["latestRelease"]


def get_prerelease_id_by_tag(tag):
    query = make_repository_query("""
      releases(first: 50, orderBy: {field: CREATED_AT, direction: DESC}) {
        edges {
          node {
            databaseId
            tagName
            isPrerelease
          }
        }
      }""")

    releases = execute_query(query)

    for edge in releases["data"]["repository"]["releases"]["edges"]:
        item = edge["node"]
        if item["isPrerelease"] and item["tagName"] == tag:
            return item["databaseId"]

    return None


def get_pull_requests(start, end, cursor=None, include_body=False):
    after = f'"{cursor}"' if cursor else "null"

    search_query = ("repo:dosbox-staging/dosbox-staging is:pr "
                    "is:merged base:main "
                    f"merged:{start}..{end} sort:updated")

    body_field = "body" if include_body else ""

    query = """
      {
        search(
          after: """ + after + """,
          first: """ + str(FETCH_PAGE_SIZE) + """,
          query: \"""" + search_query + """\",
          type: ISSUE
        ) {
          nodes {
            ... on PullRequest {
              title
              number
              author {
                login
                url
              }
              url
              createdAt
              updatedAt
              mergedAt
              """ + body_field + """
              labels(first: 20) {
                nodes {
                  name
                }
              }
            }
          }
          pageInfo {
            endCursor
            hasNextPage
          }
        }
      }
    """
    return execute_query(query)


def get_author(item):
    if "author" in item:
        author = item["author"]
        if isinstance(author, dict) and "login" in author:
            return author["login"]
    return "-"


def pull_requests_response_to_dict(response):
    nodes = response["data"]["search"]["nodes"]
    result = []

    for item in nodes:
        pr = {
            "title":     item["title"],
            "number":    item["number"],
            "author":    get_author(item),
            "url":       item["url"],
            "createdAt": item["createdAt"],
            "updatedAt": item["updatedAt"],
            "mergedAt":  item["mergedAt"],
            "labels":    ", ".join([l["name"] for l in item["labels"]["nodes"]])
        }
        if "body" in item:
            pr["body"] = item["body"]

        result.append(pr)

    return result


def fetch_all_pull_requests_since(start_time, include_body=False):
    all_items = []
    cursor = None
    item_offset = 0

    now = datetime.datetime.now(datetime.UTC).isoformat()

    while True:
        print(f"Fetching next page of pull requests, item offset: {item_offset}")
        response = get_pull_requests(start=start_time, end=now,
                                     cursor=cursor,
                                     include_body=include_body)
        items = pull_requests_response_to_dict(response)

        all_items.extend(items)
        item_offset += len(items)

        page_info = response["data"]["search"]["pageInfo"]
        if not page_info["hasNextPage"]:
            return all_items

        cursor = page_info["endCursor"]

    return all_items


def read_csv(fname):
    items = []
    with open(fname, newline="", encoding="UTF-8") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            items.append(row)
    return items


def write_csv(items, output_csv):
    with open(output_csv, "w", newline="", encoding="UTF-8") as f:
        writer = csv.DictWriter(f, items[0].keys())
        writer.writeheader()
        writer.writerows(items)


def sort_items(items):
    items.sort(key=lambda x: (x["title"].lower()))


def normalise_newlines(text):
    """ Replace Window-style newlines (CRLF) with Unix style (just LF) """
    return text.replace('\r\n', '\n').replace('\r', '\n')


def filter_category(items, filter_def):
    """ `filter_def` is an entry from FILTERS """

    remaining = []
    filtered = []

    for i in items:
        if i["author"] == "dependabot" or ("website" in i["labels"]):
            continue

        i["title"] = i["title"].strip()

        remove = filter_def["remove"] if "remove" in filter_def else True
        include = False

        if "labels" in filter_def:
            if any(label in i["labels"] for label in filter_def["labels"]):
                include = True

        if include:
            filtered.append(i)

        if not include or not remove:
            remaining.append(i)

    sort_items(filtered)

    return [filtered, remaining]


def get_contributor_list(items):
    excluded = {"-", "", "dependabot"}
    authors = {i["author"] for i in items if i["author"] not in excluded}
    return sorted(authors, key=str.casefold)


def summary_query_pull_requests(csv_fname, start_time):
    items = fetch_all_pull_requests_since(start_time)
    write_csv(items, csv_fname)
    print(f"{len(items)} pull requests written to '{csv_fname}'")


MARKDOWN_HEADER = """> [!WARNING]
> **These are the automatically generated release notes of the latest development snapshot builds!** This is not a stable release; things might blow up! 💣 🔥 💀
>
> You can download the latest development builds from here:
> https://www.dosbox-staging.org/releases/development-builds/

"""

HTML_HEADER = """
<div class="warning">
<p><strong>These are automatically generated release notes for this development
snapshot build!</strong><br>This is not a stable release; things might blow
up! 💣 🔥 💀</p>

<p>You can download the latest stable builds <a href="https://www.dosbox-staging.org/releases/">from our
website</a>.</p>
</div>
"""

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>DOSBox Staging Release Notes</title>
  <style>
    body {
      font-family: Helvetica, Arial, "Segoe UI", sans-serif;
      font-size: 17px;
      line-height: 1.6;
      max-width: 900px;
      margin: 0 auto;
      padding: 2rem;
      color: #333;
      background: #fff;
    }
    h2, h3 { margin-top: 1.5em; margin-bottom: 0.5em; }
    h2 { border-bottom: 1px solid #d0d7de; padding-bottom: 0.3em; font-size: 1.5em; }
    ul { padding-left: 2em; }
    li { margin: 0.25em 0; }
    a { color: rgb(245, 0, 86); text-decoration: none; }
    a:hover { text-decoration: underline; }
    code {
      background: #f6f8fa;
      padding: 0.2em 0.4em;
      border-radius: 3px;
      font-size: 0.9em;
    }
    em { font-style: italic; }
    .warning {
      background-color: rgba(255, 145, 0, 0.18);
      padding: 1px 15px;
      border-radius: 5px;
    }
    .authors { line-height: 1.4; }
  </style>
</head>

<body>
%CONTENT%
</body>

</html>
"""


def summary_process_pull_requests_markdown(items, markdown_fname):
    markdown = generate_markdown(items, MARKDOWN_HEADER)

    with open(markdown_fname, "w", newline="", encoding="UTF-8") as f:
        f.write(markdown)

    print(f"Release notes draft Markdown written to '{markdown_fname}'")


def append_category_markdown(markdown, items, category_name):
    markdown = f"{markdown}## Full PR list of {category_name}\n\n"

    for i in items:
        pr_link = f"[#{i['number']}]({i['url']})"
        item_md = f"  - {i['title']} ({pr_link})"
        if "backport" in i["labels"]:
            item_md += " _[backport]_"

        markdown += item_md + "\n"

    return f"{markdown}\n\n"


def append_contributor_list(markdown, items):
    markdown = f'{markdown}## Commit authors\n\n<div class="authors">\n<ul>\n'

    for i in get_contributor_list(items):
        markdown = f"{markdown}  <li>{i}</li>\n"

    return f"{markdown}</ul>\n</div>\n"


def generate_markdown(items, header):
    remaining = items
    markdown = header

    for filter_def in FILTERS:
        [filtered, remaining] = filter_category(remaining, filter_def)
        if filtered:
            markdown = append_category_markdown(markdown, filtered,
                                                filter_def["title"])

    sort_items(remaining)

    if remaining:
        markdown = append_category_markdown(markdown, remaining,
                                            "other changes")

    markdown = append_contributor_list(markdown, items)
    return markdown


def summary_process_pull_requests_csv(items, csv_fname):
    remaining = items
    result = []

    for filter_def in FILTERS:
        [filtered, remaining] = filter_category(remaining, filter_def)

        for item in filtered:
            item["category"] = filter_def["category"]
            result.append(item)

    sort_items(remaining)

    for item in remaining:
        item["category"] = "other"
        result.append(item)

    write_csv(result, csv_fname)
    print(f"Categorised pull requests CSV written to '{csv_fname}'")


def summary_process_pull_requests_html(items, version_tag, html_fname):
    markdown = generate_markdown(items, HTML_HEADER)

    # Convert markdown to HTML
    html_content = markdown2.markdown(markdown)

    header = f'<h1>DOSBox Staging {version_tag}</h1>'
    html_content = header + html_content

    # Wrap in HTML template
    html = HTML_TEMPLATE.replace("%CONTENT%", html_content)

    with open(html_fname, "w", newline="", encoding="UTF-8") as f:
        f.write(html)

    print(f"Release notes draft HTML written to '{html_fname}'")


def make_upsert_release_payload(tag, name, description):
    return json.dumps({
        "tag_name": tag,
        "target_commitish": "main",
        "name": name,
        "body": description,
        "draft": False,
        "prerelease": True,
        "generate_release_notes": False
    })


def update_release(release_id, tag, name, description):
    url = f"{RELEASES_BASE_URL}/{release_id}"
    data = make_upsert_release_payload(tag, name, description)

    r = requests.patch(url, headers=create_headers(), data=data,
                       timeout=HTTP_TIMEOUT_SEC)

    if r.status_code != 200:
        print_api_error(data, r)
        sys.exit(1)

    return json.loads(r.text)


def create_release(tag, name, description):
    url = RELEASES_BASE_URL
    data = make_upsert_release_payload(tag, name, description)

    r = requests.post(url, headers=create_headers(), data=data,
                      timeout=HTTP_TIMEOUT_SEC)

    if r.status_code != 201:
        print_api_error(data, r)
        sys.exit(1)

    return json.loads(r.text)


def summary_publish_prerelease(markdown_file, tag):
    release_id = get_prerelease_id_by_tag(tag)

    with open(markdown_file, encoding="UTF-8") as f:
        description = f.read()

    name = f"{tag} release notes preview"

    if release_id:
        update_release(release_id, tag, name, description)
    else:
        create_release(tag, name, description)


def website_query_pull_requests(json_fname, start_time):
    items = fetch_all_pull_requests_since(start_time, include_body=True)
    write_json(items, json_fname)
    print(f"{len(items)} pull requests written to '{json_fname}'")


def website_process_pull_requests(json_fname, version, markdown_fname):
    items = read_json(json_fname)
    items = [i for i in items if "backport" not in i["labels"]]

    markdown = website_generate_markdown(items, version)

    with open(markdown_fname, "w", encoding="UTF-8") as f:
        f.write(markdown)

    print(f"Website release notes draft written to '{markdown_fname}'")


TEMPLATE_PLACEHOLDER_PHRASES = [
    "If this is a user-facing change",
    "Remove the section if no notes are needed",
]


def categorize_pull_requests(items):
    """Categorize PRs using FILTERS, returning a dict of category → PR list."""
    categories = {}
    remaining = items

    for filter_def in FILTERS:
        [filtered, remaining] = filter_category(remaining, filter_def)
        if filtered:
            categories[filter_def["category"]] = filtered

    sort_items(remaining)
    if remaining:
        categories["other"] = remaining

    return categories


def append_collapsible_pr_list(md, items, title):
    """Append a collapsible PR list section."""
    md += f'??? note "Full PR list of {title}"\n\n'
    for item in items:
        md += f"    - {item['title']} (#{item['number']})\n"
    md += "\n\n"
    return md


def extract_release_notes(body):
    """Extract the 'Release notes' section from a PR description."""
    if not body:
        return None

    body = normalise_newlines(body)

    match = re.search(r"^#\s+Release notes\s*$", body, re.MULTILINE |
                      re.IGNORECASE)
    if not match:
        return None

    rest = body[match.end():]

    # Find the next first level heading or end of string
    next_heading = re.search(r"^#\sManual testing\s*$", rest, re.MULTILINE)
    content = rest[:next_heading.start()] if next_heading else rest
    content = normalise_newlines(content.strip())

    if not content:
        return None

    for phrase in TEMPLATE_PLACEHOLDER_PHRASES:
        if phrase in content:
            return None

    return content


def append_narrative_prs(md, items):
    """Append release notes extracted from PR descriptions."""
    for item in items:
        notes = extract_release_notes(item.get("body", ""))
        if notes:
            md += f"## {item['title']} (#{item['number']})\n\n"
            md += f"{notes}\n\n\n"
    return md


# pylint: disable=too-many-locals
def website_generate_markdown(items, version):
    categories = categorize_pull_requests(items)
    filter_titles = {f["category"]: f["title"] for f in FILTERS}
    short_version = version.rsplit(".", 1)[0] if "." in version else version

    md = f"""\
# {version} release notes

**Release date:** YYYY-MM-DD

## Summary

TODO: Write summary of highlights


## Downloads

Start by downloading the latest version:

<div class="compact" markdown>

- [Windows](../windows.md)
- [macOS](../macos.md)
- [Linux](../linux.md)

</div>

TODO: Add upgrade instructions


"""

    # Standard sections
    standard_sections = [
        ("game-compatibility", "Game compatibility fixes"),
        ("graphics",           "Graphics"),
        ("sound",              "Sound"),
        ("input-handling",     "Input"),
        ("dos-integration",    "DOS integration"),
    ]

    for category, heading in standard_sections:
        prs = categories.get(category, [])
        if prs:
            md += f"## {heading}\n\n"
            md = append_narrative_prs(md, prs)
            md = append_collapsible_pr_list(md, prs,
                                            filter_titles[category])

    # General section: narrative from misc-enhancements/misc-fixes,
    # collapsible lists for all "general" sub-categories
    general_narrative = ["misc-enhancements", "misc-fixes"]

    general_collapsible_only = ["documentation", "project-maintenance",
                                "other"]

    all_general = general_narrative + general_collapsible_only

    if any(categories.get(cat) for cat in all_general):
        md += "## General\n\n"

        for cat in general_narrative:
            md = append_narrative_prs(md, categories.get(cat, []))

        for cat in all_general:
            prs = categories.get(cat, [])
            if prs:
                title = filter_titles.get(cat, "other changes")
                md = append_collapsible_pr_list(md, prs, title)

    # Localisation
    loc_prs = categories.get("localisation", [])
    if loc_prs:
        md += "## Localisation\n\n"
        md = append_narrative_prs(md, loc_prs)
        md = append_collapsible_pr_list(md, loc_prs,
                                        filter_titles["localisation"])

    # Contributors & Thank you
    contributors = "\n".join(f"- {a}" for a in
                             get_contributor_list(items))

    md += f"""\
## Contributors

### {short_version} commit authors

<div class="compact" markdown>

{contributors}

</div>


## Thank you

We are grateful for all the community contributions and the original DOSBox
project, on which DOSBox Staging is based.
"""

    return md


def read_json(fname):
    with open(fname, encoding="UTF-8") as f:
        return json.load(f)


def write_json(items, output_json):
    with open(output_json, "w", encoding="UTF-8") as f:
        json.dump(items, f, indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(
        formatter_class=CustomFormatter,
        description=__doc__
    )
    setup_arg_parser(parser)
    args = parser.parse_args()

    actions_requiring_token = [
        "summary.query", "summary.process_markdown", "summary.process_html",
        "summary.process_csv", "summary.publish", "website.query"
    ]

    global GITHUB_ACCESS_TOKEN
    GITHUB_ACCESS_TOKEN = os.getenv("GITHUB_ACCESS_TOKEN")

    if args.ACTION in actions_requiring_token and not GITHUB_ACCESS_TOKEN:
        print("Error: GITHUB_ACCESS_TOKEN env var is empty")
        sys.exit(1)

    match args.ACTION:
        case "summary.query":
            start_time = args.start_time or get_default_start_time()
            if not start_time:
                parser.error("--start_time must be specified")
            summary_query_pull_requests(args.out_pull_requests_csv,
                                        start_time)

        case "summary.process_markdown":
            items = read_csv(args.in_pull_requests_csv)
            summary_process_pull_requests_markdown(items,
                                                   args.out_markdown)

        case "summary.process_html":
            items = read_csv(args.in_pull_requests_csv)
            summary_process_pull_requests_html(items,
                                               args.process_version_tag,
                                               args.out_html)

        case "summary.process_csv":
            items = read_csv(args.in_pull_requests_csv)
            summary_process_pull_requests_csv(items,
                                              args.out_categorised_csv)

        case "summary.publish":
            summary_publish_prerelease(args.in_markdown,
                                       args.publish_version_tag)

        case "website.query":
            start_time = args.start_time or get_default_start_time()
            if not start_time:
                parser.error("--start_time must be specified")
            website_query_pull_requests(args.out_pull_requests_json,
                                        start_time)

        case "website.process_markdown":
            website_process_pull_requests(args.in_pull_requests_json,
                                          args.process_version_tag,
                                          args.out_markdown)


if __name__ == "__main__":
    main()
