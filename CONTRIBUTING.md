Contributing
============

If you would like to contribute code to this project you can do so through GitHub by forking the repository and sending a pull request. Before RDK accepts your code into the project you must sign the RDK Contributor License Agreement (CLA).

Building RDK ARM Starboard
-------------------------

The script downloads `oecore-installer.tar.gz` from
`https://artifactory.rdkcentral.com/artifactory/cov_larboard/oecore-installer.tar.gz`
using Bearer authentication and extracts the SDK installer into `sdk-installer/`.
Configure the `ARTIFACTORY_TOKEN` Actions secret for GitHub builds, or supply
the environment variable through your local/internal CI secret mechanism.
The token is required only when downloading a fresh SDK. Do not commit the
token, downloaded archive, or installed SDK, and do not enable shell tracing
(`set -x`) when running the authenticated download.
The installer is not tracked by Git; repository checkout does not need to
download the SDK through Git LFS.

On a Linux host suitable for the Yocto SDK, run:

```sh
bash cov_build.sh
```

The script installs the SDK into `oecore-x86_64/`, checks out the pinned
depot_tools revision and Cobalt `27.lts.3`, syncs dependencies, and builds
Larboard's ARM Starboard library. Network access is required for the source
checkouts, dependencies, and build tools. The build follows the architect's
environment setup and gclient/GN/Ninja commands; it does not impose a separate
system `gcc` or `g++` prerequisite check. Existing SDK and source checkouts
are reused; an incomplete SDK installation or conflicting Starboard path
is reported as an error rather than overwritten.

After sourcing the SDK, the script points SSL and Git at the host CA bundle
`/etc/ssl/certs/ca-certificates.crt` so dependency downloads do not use the
SDK build machine's certificate path. This does not change global Git
configuration or disable certificate verification.

The output is
`cobalt-build/src/out/rdk-arm_gold/starboard/obj/third_party/starboard/rdk/shared/libstarboard_platform.a`.
Set `NINJA_JOBS` to change the default of four parallel build jobs.