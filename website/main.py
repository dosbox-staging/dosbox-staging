"""
MkDocs macros plugin definitions.

Provides Jinja2 macros that can be called from Markdown pages using
{{ macro_name(...) }} syntax.
"""

import os
import re
from html import unescape

def _caption_to_alt(caption):
    """
    Derive plain-text alt from a caption that may contain HTML tags,
    markdown links, and typographic markdown.

    - <br> / <br/> → space (it represents a line break in the caption)
    - All other HTML tags stripped, inner text kept
    - Markdown links [text](url) → text
    - --- → em dash, -- → en dash
    - HTML entities unescaped
    - Whitespace collapsed
    """
    text = re.sub(r"<br\s*/?>", " ", caption, flags=re.IGNORECASE)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    text = text.replace("---", "\u2014")
    text = text.replace("--", "\u2013")
    text = unescape(text)
    text = re.sub(r"\s+", " ", text).strip()
    return text


def define_env(env):
    """Called by mkdocs-macros to register custom macros."""

    offline = env.conf["extra"].get("offline_mode", False)

    # pylint: disable=too-many-arguments
    # pylint: disable=too-many-locals
    @env.macro
    def figure(src, caption=None, *, alt=None, small=True,
               small_name=None, width=None, style=None, lightbox=True):
        """
        Generate a <figure> with optional GLightbox lightbox support.
        Works in both online and offline documentation builds.

        Args:
            src:        URL of the full-size image (required).

            caption:    Figcaption text (optional). Passed through inside
                        <figcaption markdown> so MkDocs renders markdown
                        and smarty typography (--- to em dash, etc.).
                        Also used as the alt text source by default.

            alt:        Explicit alt text override (optional). When
                        omitted, derived from caption by stripping HTML
                        tags, markdown links, and converting typography.
                        When both caption and alt are omitted, the image
                        gets an empty alt attribute.

            small:      Whether a '-small' thumbnail variant exists
                        (default True). When True, the macro auto-derives
                        the thumbnail URL by inserting '-small' before
                        the file extension (e.g. foo.jpg → foo-small.jpg).
                        Only affects online builds with lightbox enabled.

            small_name: Explicit thumbnail filename (optional). Replaces
                        the filename in src with this value, keeping the
                        same base path. Use when the small variant has a
                        different extension (e.g. 'img-small.jpg' for a
                        full-size 'img.png'). Implies small=True.

            width:      Image width attribute, e.g. "80%" or "400".

            style:      Inline CSS for the image element, e.g.
                        "width: 25rem; margin: 1.5rem 0;".

            lightbox:   Whether clicking opens the image in a GLightbox
                        lightbox (default True). When False, adds the
                        .skip-lightbox class to prevent wrapping.

        Offline mode (OFFLINE=true):

            When the dual-image lightbox pattern would be used (small=True
            or small_name given, lightbox=True), we instead emit a plain
            <img src="FULL"> without the <a class="glightbox"> wrapper.
            The Material privacy plugin downloads every external <img src>
            it finds, so it fetches the full-size image and rewrites the
            src to a local path. mkdocs-glightbox then auto-wraps it.

            This means offline docs use full-size images as thumbnails
            (slightly heavier pages), but eliminates the need for any
            post-build script to download and localise the full-size
            images from <a href> (which the privacy plugin ignores).

            All other forms (small=False, lightbox=False) produce the
            same markup in both online and offline mode, since they
            already put the image URL in <img src> where the privacy
            plugin can reach it.
        """
        # Resolve alt text: explicit override > derived from caption > empty
        if alt is None:
            alt_text = _caption_to_alt(caption) if caption else ""
        else:
            alt_text = alt

        # Build the image attribute string: { loading=lazy ... }
        attrs = ["loading=lazy"]
        if width:
            attrs.append(f"width={width}")
        if style:
            attrs.append(f'style="{style}"')

        # Dual-image lightbox mode: separate small thumbnail and
        # full-size lightbox image. Only used in online builds when
        # lightbox is enabled and a small variant is available.
        use_dual = lightbox and (small or small_name) and not offline

        if use_dual:
            if small_name:
                base_path = src.rsplit("/", 1)[0]
                thumb_src = f"{base_path}/{small_name}"
            else:
                base, ext = os.path.splitext(src)
                thumb_src = f"{base}-small{ext}"

            attrs.append(".skip-lightbox")
            attr_str = " ".join(attrs)

            lines = [
                '<figure markdown>',
                f'  <a class="glightbox" href="{src}">',
                f'    ![{alt_text}]({thumb_src}){{ {attr_str} }}',
                '  </a>',
            ]
        else:
            # Single-image mode. In offline builds the privacy plugin
            # downloads the image; mkdocs-glightbox auto-wraps it
            # for lightbox (unless lightbox=False).
            if not lightbox:
                attrs.append(".skip-lightbox")

            attr_str = " ".join(attrs)

            lines = [
                '<figure markdown>',
                f'  ![{alt_text}]({src}){{ {attr_str} }}',
            ]

        if caption:
            lines.append(f'  <figcaption markdown>{caption}</figcaption>')

        lines.append('</figure>')
        return "\n".join(lines)
