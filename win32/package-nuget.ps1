<#
.SYNOPSIS

   Create NuGet package using CoApp


.DESCRIPTION

   A full build must be completed, to populate output directories, before

   running this script.

   Use build.bat to build


   Requires CoApp
#>

param ([string]$build_version = "0.9.3-pre1")

# Make the tools available for later PS scripts to use.
$env:PSModulePath = $env:PSModulePath + ';C:\Program Files (x86)\Outercurve Foundation\Modules'
Import-Module CoApp

# This is the CoApp .autopkg file to create.
$autopkgFile = "librdkafka.autopkg"

# Get the ".autopkg.template" file, replace "@version" with the Appveyor version number, then save to the ".autopkg" file.
cat ($autopkgFile + ".template") | % { $_ -replace "@version", $build_version } > $autopkgFile

# Use the CoApp tools to create NuGet native packages from the .autopkg.
Write-NuGetPackage $autopkgFile

