"""MkDocs hook: validate versions.json consistency at build time."""

import json
import logging
from pathlib import Path

log = logging.getLogger("mkdocs.hooks.versions")


def on_config(config):
    """Validate version.json consistency."""

    path = Path(config["docs_dir"]) / "versions.json"
    if not path.exists():
        return

    sections = json.loads(path.read_text())
    all_versions = {v for versions in sections.values() for v in versions}
    highest = max(all_versions,
                  key=lambda v: tuple(int(x) for x in v.split(".")))

    for section, versions in sections.items():
        if highest not in versions:
            log.warning(
                "versions.json: latest version %s is missing from '%s'",
                highest, section
            )
