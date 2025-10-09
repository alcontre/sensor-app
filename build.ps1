<#
   Build helper script for configuring and compiling the project with Visual Studio 2019.
   - Defaults to the RelWithDebInfo configuration (optimized build with debug symbols).
   - Creates or reuses an out-of-source build directory under the repository root.
   - Invokes CMake for both configure and build steps, honoring the supplied parameters.
#>

param(
    # CMake build configuration to compile (RelWithDebInfo by default).
    [ValidateSet("Debug", "RelWithDebInfo")]
    [string]$Configuration = "RelWithDebInfo",
    # CMake generator to use (Visual Studio 2019 or Ninja).
    [ValidateSet("Visual Studio 16 2019", "Ninja")]
    [string]$Generator = "Ninja",
    # Target architecture passed to CMake (x64 only, applicable to Visual Studio).
    [ValidateSet("x64")]
    [string]$Architecture = "x64",
    # Folder created under the repository root that stores generated build files.
    [string]$BuildDirName = "build-vs2019"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Determine repository root (where this script lives) and build directory path.
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $repoRoot $BuildDirName

if (-not (Test-Path $buildDir)) {
    # Ensure the build directory exists for out-of-source CMake builds.
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Configure the project with the requested generator and architecture.
Write-Host "Configuring project with $Generator in '$BuildDirName'..."

$configureArgs = @("-S", $repoRoot, "-B", $buildDir, "-G", $Generator, "--log-level=NOTICE")
if ($Generator -like "Visual Studio*") {
    $configureArgs += @("-A", $Architecture)
}

cmake @configureArgs

# Build the chosen configuration.
Write-Host "Building configuration '$Configuration'..."

cmake --build $buildDir --config $Configuration

# Report completion.
Write-Host "Build completed successfully."