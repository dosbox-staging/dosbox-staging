# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=unused-argument

def _trim(items, level, max_depth):
    for item in items:
        if level >= max_depth:
            item.children = []
        else:
            _trim(item.children, level + 1, max_depth)


def on_page_context(context, page, config, nav):
    depth = page.meta.get("toc_depth")
    if depth is not None:
        _trim(page.toc, 0, int(depth))
