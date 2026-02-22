#!/usr/bin/env python3

# SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
# SPDX-License-Identifier: MIT

"""
Create release notes draft for DOSBox Staging.
"""

# pylint: disable=invalid-name
# pylint: disable=missing-docstring
# pylint: disable=global-statement

import argparse
import csv
import json
import os
import sys

from datetime import datetime

import requests


GITHUB_ORG          = "dosbox-staging"
GITHUB_REPO         = "dosbox-staging"
GITHUB_API_URL      = "https://api.github.com"
GITHUB_ACCESS_TOKEN = ""

RELEASES_BASE_URL = f"{GITHUB_API_URL}/repos/{GITHUB_ORG}/{GITHUB_REPO}/releases"

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


def setup_arg_parser(parser):
    parser.add_argument(
        "ACTION",
        choices=["query", "process", "publish"],
        help="""query
  Queries information about all pull requests merged to the 'main'
  branch since the last public DOSBox Staging release and writes the
  results to a CSV file.

process
  Processes the CSV output of the query action and generates the release
  notes draft either in Markdown or CSV format.

publish
  Publishes (creates or updates) the release notes draft.
""",
    )

    query_args = parser.add_argument_group(title="query arguments")
    query_args.add_argument(
        "-s",
        "--start_time",
        help="include pull requests after this datetime"
    )
    query_args.add_argument(
        "-p",
        "--pull_requests_csv",
        help="output CSV file containing the pull requests"
    )

    process_args = parser.add_argument_group(title="process arguments")

    process_args.add_argument(
        "-i",
        "--input_csv",
        help="input CSV file containing the pull requests"
    )

    process_args.add_argument(
        "-m",
        "--markdown_file",
        help="write categorised PRs to a Markdown file"
    )

    process_args.add_argument(
        "-c",
        "--csv_file",
        help="write categorised PRs to a CSV file"
    )

    publish_args = parser.add_argument_group(title="publish arguments")

    publish_args.add_argument(
        "-n",
        "--release_notes_file",
        help="releate notes Markdown file"
    )

    publish_args.add_argument(
        "-t",
        "--version_tag",
        help="version tag of the release notes draft (e.g., v0.83.0-alpha)"
    )


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


def get_pull_requests(start, end, cursor=None):
    after = f'"{cursor}"' if cursor else "null"

    search_query = ("repo:dosbox-staging/dosbox-staging is:pr "
                    "is:merged base:main "
                    f"merged:{start}..{end} sort:updated")

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
        result.append({
            "title":     item["title"],
            "number":    item["number"],
            "author":    get_author(item),
            "url":       item["url"],
            "createdAt": item["createdAt"],
            "updatedAt": item["updatedAt"],
            "mergedAt":  item["mergedAt"],
            "labels":    ", ".join([l["name"] for l in item["labels"]["nodes"]])
        })

    return result


def fetch_all_pull_requests_since(start_time):
    all_items = []
    cursor = None
    item_offset = 0

    now = datetime.utcnow().isoformat()

    while True:
        print(f"Fetching next page of pull requests, item offset: {item_offset}")
        response = get_pull_requests(start=start_time, end=now, cursor=cursor)
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

    filtered.sort(key=lambda x: (x["title"].lower()))

    return [filtered, remaining]


def get_contributor_list(items):
    res = map(lambda i: i["author"], items)
    res = filter(lambda s: s not in ["-", "", "dependabot"], res)
    res = list(set(res))
    res.sort(key=str.casefold)
    return res


def append_contributor_list(markdown, items):
    markdown = f"{markdown}## Commit authors\n\n"

    for i in get_contributor_list(items):
        markdown = f"{markdown}  - {i}\n"

    return f"{markdown} \n\n"


def process_pull_requests_csv(items, csv_fname):
    remaining = items
    result = []

    for filter_def in FILTERS:
        [filtered, remaining] = filter_category(remaining, filter_def)

        for item in filtered:
            item["category"] = filter_def["category"]
            result.append(item)

    for item in remaining:
        item["category"] = "other"
        result.append(item)

    write_csv(result, csv_fname)


def append_category_markdown(markdown, items, category_name):
    markdown = f"{markdown}## Full PR list of {category_name}\n\n"

    for i in items:
        item_md = f"  - {i['title']} (#{i['number']})"
        if "backport" in i["labels"]:
            item_md += " _[backport]_"

        markdown += item_md + "\n"

    return f"{markdown}\n\n"


PRERELEASE_WARNING = """> [!WARNING]
> **Auto-generated release notes preview** — This is not a release.
> No binaries are available. These notes are updated automatically as PRs are merged.

"""


def process_pull_requests_markdown(items, markdown_fname):
    remaining = items
    markdown = PRERELEASE_WARNING

    for filter_def in FILTERS:
        [filtered, remaining] = filter_category(remaining, filter_def)
        if filtered:
            markdown = append_category_markdown(markdown, filtered,
                                                filter_def["title"])

    if remaining:
        markdown = append_category_markdown(markdown, remaining,
                                            "other changes")

    markdown = append_contributor_list(markdown, items)

    with open(markdown_fname, "w", newline="", encoding="UTF-8") as f:
        f.write(markdown)


def query_pull_requests(csv_fname, start_time):
#    latest_release = get_latest_public_release()

#    latest_release_name = latest_release["name"]
#    start_time = latest_release["publishedAt"]
#    print(f"Latest release name: {latest_release_name}, published at: {start_time}")

    items = fetch_all_pull_requests_since(start_time)
    write_csv(items, csv_fname)

    print(f"{len(items)} pull requests written")


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


def create_release(tag, name, description):
    url = RELEASES_BASE_URL
    data = make_upsert_release_payload(tag, name, description)

    r = requests.post(url, headers=create_headers(), data=data,
                      timeout=HTTP_TIMEOUT_SEC)

    if r.status_code != 201:
        print_api_error(data, r)
        sys.exit(1)

    return json.loads(r.text)


def update_release(release_id, tag, name, description):
    url = f"{RELEASES_BASE_URL}/{release_id}"
    data = make_upsert_release_payload(tag, name, description)

    r = requests.patch(url, headers=create_headers(), data=data,
                       timeout=HTTP_TIMEOUT_SEC)

    if r.status_code != 200:
        print_api_error(data, r)
        sys.exit(1)

    return json.loads(r.text)


def publish_prerelease(markdown_file, tag):
    release_id = get_prerelease_id_by_tag(tag)

    with open(markdown_file, encoding="UTF-8") as f:
        description = f.read()

    name = f"{tag} release notes preview"

    if release_id:
        update_release(release_id, tag, name, description)
    else:
        create_release(tag, name, description)


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        description=__doc__
    )
    setup_arg_parser(parser)
    args = parser.parse_args()

    global GITHUB_ACCESS_TOKEN
    GITHUB_ACCESS_TOKEN = os.getenv("GITHUB_ACCESS_TOKEN")

    if not GITHUB_ACCESS_TOKEN:
        print("Error: GITHUB_ACCESS_TOKEN env var is empty")
        sys.exit(1)

    match args.ACTION:
        case "query":
            if not args.start_time:
                parser.error("start time must be specified with -s")

            if not args.pull_requests_csv:
                parser.error("output CSV file must be specified with -p")

            query_pull_requests(args.pull_requests_csv, args.start_time)

        case "process":
            if not args.input_csv:
                parser.error("input CSV file must be specified with -i")

            if not args.markdown_file and not args.csv_file:
                parser.error("at least one of -m or -c must be specified")

            items = read_csv(args.input_csv)

            if args.markdown_file:
                process_pull_requests_markdown(items, args.markdown_file)

            if args.csv_file:
                process_pull_requests_csv(items, args.csv_file)

        case "publish":
            if not args.release_notes_file:
                parser.error("release notes file must be specified with -n")

            if not args.version_tag:
                parser.error("release tag must be specified with -t")

            publish_prerelease(args.release_notes_file, args.version_tag)


if __name__ == "__main__":
    main()
