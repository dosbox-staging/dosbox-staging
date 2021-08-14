This spec file is only an example; you can use it as a blueprint for
other RPM-based distributions.

Prepare rpmbuild tree:

    $ rpmdev-setuptree

Build rpm and srpm using specfile and auto-downloaded sources:

    $ ln -s "$(pwd)/dosbox-staging.spec" ~/rpmbuild/SPECS/
    $ cd ~/rpmbuild/SPECS/
    $ spectool -g -R dosbox-staging.spec
    $ rpmbuild -bb dosbox-staging.spec
    $ rpmbuild -bs dosbox-staging.spec

Do some verification:

    $ rpmlint ~/rpmbuild/RPMS/x86_64/dosbox-staging-*
    $ rpmlint ~/rpmbuild/SRPMS/dosbox-staging-*

Fedora build (via Koji buildsystem):

    $ sudo dnf install -y fedora-packager
    $ echo "<user>" > ~/.fedora.upn
    $ kinit <user>@FEDORAPROJECT.ORG
    $ koji build --scratch rawhide ~/rpmbuild/SRPMS/dosbox-staging-0.77.0-*

Koji will print build status and URL (Task info).

Fedora package update:

    $ fedpkg import <new src.rpm file>

Make a commit, push it to the relevant branch, then:
    
    $ fedpkg build

Finally, for released Fedora versions, request a package update via https://bodhi.fedoraproject.org/
