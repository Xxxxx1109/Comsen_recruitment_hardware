param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [string]$ToolsRoot = (Join-Path $PSScriptRoot '../Comsen-tools')
)
$ErrorActionPreference = 'Stop'
$ToolsRoot = (Resolve-Path -LiteralPath $ToolsRoot).Path
$gccBin = Join-Path $ToolsRoot 'gcc/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin'
$cmake = Join-Path $ToolsRoot 'cmake/cmake-3.31.8-windows-x86_64/bin/cmake.exe'
$ninja = Join-Path $ToolsRoot 'ninja/ninja.exe'
foreach ($tool in @($cmake, (Join-Path $gccBin 'arm-none-eabi-gcc.exe'), $ninja)) {
    if (-not (Test-Path -LiteralPath $tool)) { throw "Missing tool: $tool" }
}
# CMake/Ninja on Windows handle the executable path more reliably without spaces.
$fso = New-Object -ComObject Scripting.FileSystemObject
$ninja = $fso.GetFile($ninja).ShortPath.Replace('\', '/')
$originalPath = $env:PATH
try {
    $env:PATH = "$gccBin;$(Split-Path -Parent $ninja);$originalPath"
    $source = Join-Path $PSScriptRoot 'example'
    $build = Join-Path $source "build/Windows$Configuration"
    & $cmake -S $source -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build $build --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
    Write-Output "Firmware: $build/software.hex"
} finally {
    $env:PATH = $originalPath
}
