# Steps to Refresh the Coverity Software

1. Every 6 months check <https://scan.coverity.com/download> for
   a package newer than 2019.03
1. Download it, extract it, and remove unnecessary content:

   ``` shell
   rm -rf closure-compiler jars jdk11 jre node support-angularjs
   cd bin
   rm *java* *js* *php* *python* *ruby*
   ```

1. Compress and Checksum the archive:

   ``` shell
   tar -cv cov-analysis-linux64-YYYY-MM | zstd -19 > cov-analysis-linux64-YYYY-MM.tar.zst
   sha256sum cov-analysis-linux64-YYYY-MM.tar.zst
   ```

1. Upload it to Google Drive, right-click -> shareable link ->
   get the ID value from the URL.
1. Update the `TARBALL_SHA256` and `PACKAGE_VERSION` environment
   variables in the Workflow YAML.
1. Update the Repository's secret `GoogleDriveId`.
