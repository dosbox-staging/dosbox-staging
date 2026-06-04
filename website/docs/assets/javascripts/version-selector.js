document.addEventListener("DOMContentLoaded", function () {
    var path = window.location.pathname;

    var topics = document.querySelectorAll(".md-header__topic");

    // Extract the deploy prefix (everything before the version segment).
    // The non-greedy (.*?) grows via backtracking until /X.XX/ is found.
    //
    //   /0.83/manual/about/                → prefix = ""
    //   /preview/dev/0.83/manual/about/    → prefix = "/preview/dev"
    //   /some/prefix/0.83/manual/about/    → prefix = "/some/prefix"

    var match = path.match(/^(.*?)\/[0-9]\.[0-9]{2}\//);

    if (topics.length > 0 && match) {
        var deployPrefix = match[1];

        fetch(deployPrefix + "/versions.json")
            .then(function (r) {
                if (!r.ok) throw new Error(r.status);
                return r.json();
            })
            .then(function (sections) { populate(sections, deployPrefix); })
            .then(adjustScrollMargin)
            .catch(adjustScrollMargin);
    } else {
        adjustScrollMargin();
    }


    function populate(sections, deployPrefix) {
        // Detect current version and section from URL
        var currentVersion = null;
        var currentSection = null;
        var restOfPath = null;

        var sectionNames = Object.keys(sections);

        for (var i = 0; i < sectionNames.length; i++) {
            var section = sectionNames[i];
            var versions = sections[section];

            for (var j = 0; j < versions.length; j++) {
                var prefix = deployPrefix + "/" + versions[j] + "/" + section + "/";

                if (path.startsWith(prefix)) {
                    currentVersion = versions[j];
                    currentSection = section;
                    restOfPath = path.slice(prefix.length);
                    break;
                }
            }
            if (currentVersion) {
                break;
            }
        }

        // Not on a versioned page
        if (!currentVersion) {
            return;
        }

        var availableVersions = sections[currentSection];

        // Build the version selector and inject it into the header
        var container = document.createElement("div");
        container.className = "md-version";
        container.setAttribute("role", "navigation");
        container.setAttribute("aria-label", "Select version");

        var btn = document.createElement("button");
        btn.className = "md-version__current";
        btn.setAttribute("aria-label", "Select version");
        btn.textContent = currentVersion;
        container.appendChild(btn);

        var list = document.createElement("ul");
        list.className = "md-version__list";

        for (var i = 0; i < availableVersions.length; i++) {
            var v = availableVersions[i];
            var li = document.createElement("li");
            li.className = "md-version__item";

            var a = document.createElement("a");
            a.className = "md-version__link";
            a.textContent = v;
            a.href = deployPrefix + "/" + v + "/" + currentSection + "/" + restOfPath;

            if (v === currentVersion) {
                a.setAttribute("aria-current", "page");
            }

            li.appendChild(a);
            list.appendChild(li);
        }

        container.appendChild(list);
        for (var t = 0; t < topics.length; t++) {
            topics[t].appendChild(
                t === 0 ? container : container.cloneNode(true)
            );
        }

        // Show outdated banner if not on the latest version
        var latestVersion = availableVersions[0];

        if (currentVersion !== latestVersion) {
            var aside = document.createElement("aside");
            aside.className = "md-banner md-banner--warning";
            aside.setAttribute("data-md-color-scheme", "default");

            var inner = document.createElement("div");
            inner.className = "md-banner__inner md-grid md-typeset";

            var latestHref = deployPrefix + "/" + latestVersion +
                "/" + currentSection + "/" + restOfPath;

            inner.appendChild(document.createTextNode(
                "You're viewing an outdated version of this document. "
            ));

            var link = document.createElement("a");
            link.href = latestHref;

            var strong = document.createElement("strong");
            strong.textContent = "Click here to go to the latest version.";
            link.appendChild(strong);

            inner.appendChild(link);

            aside.appendChild(inner);

            var header = document.querySelector(".md-header");
            header.parentNode.insertBefore(aside, header);
        }
    }

    // Expose banner heights so the header and warning banner can stack
    // beneath the dev-site banner via CSS, and bump --md-scroll-margin so
    // anchor jumps clear the banners.
    function adjustScrollMargin() {
        var devBanner = document.querySelector(".dev-site:not([hidden])");
        var versionBanner = document.querySelector(".md-banner--warning");

        function measure(el) {
            return el ? el.getBoundingClientRect().height : 0;
        }

        function updateHeights() {
            document.documentElement.style.setProperty(
                "--dev-banner-height", measure(devBanner) + "px"
            );
            document.documentElement.style.setProperty(
                "--warning-banner-height", measure(versionBanner) + "px"
            );
        }

        updateHeights();

        // Re-measure after web fonts load and on viewport resize, since
        // banner heights depend on both.
        if (document.fonts && document.fonts.ready) {
            document.fonts.ready.then(updateHeights);
        }
        if (typeof ResizeObserver !== "undefined") {
            var observer = new ResizeObserver(updateHeights);
            if (devBanner) observer.observe(devBanner);
            if (versionBanner) observer.observe(versionBanner);
        }

        var extra = measure(devBanner) + measure(versionBanner);
        if (extra > 0) {
          var current = getComputedStyle(document.documentElement)
            .getPropertyValue("--md-scroll-margin");

          var base = parseFloat(current) || 0;

          document.documentElement.style.setProperty(
              "--md-scroll-margin", (base + extra) + "px"
          );
        }
    }
});
