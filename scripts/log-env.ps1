# To use this script in GitHub Actions job:
#
#  - name: "Log environment"
#    shell: pwsh
#    run: .\scripts\log-env.ps1
#
# This script works only on Windows.
# see also script: log-env.sh

Set-PSDebug -Trace 1
systeminfo
gci env:
Set-PSDebug -Trace 0
