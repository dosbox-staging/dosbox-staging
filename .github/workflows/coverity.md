# Steps to Refresh the Coverity Software

1. Every 6 months check <https://scan.coverity.com/download> for
   a package newer than 2020.09

2. Download it, extract it, and remove unnecessary content:

   ``` shell
   rm -rf closure-compiler config/java* dotnet go jars jdk* \
          jre maven yarn node support-angularjs template-da
   cd bin
   rm -rf CovEmit* *cs* *-go* *-dotnet* *java* *js* *php* \
          *python* *razor* *ruby* *vb*
   strip cov-*
   ```

3. Compress and Checksum the archive:

   ``` shell
   tar -cv cov-analysis-linux64-YYYY-MM | zstd -19 > coverity.tar.zst
   sha256sum coverity.tar.zst
   ```

4. Edit Coverity's workflow YAML and update the following:
    - Download URL (if changed)
    - `TARBALL_SHA256`
    - `PACKAGE_VERSION`
