$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$clangFormat = $null
if (Get-Command clang-format -ErrorAction SilentlyContinue) {
  $clangFormat = "clang-format"
} else {
  $paths = @(
    "C:\Program Files\LLVM\bin\clang-format.exe",
    "C:\Program Files (x86)\LLVM\bin\clang-format.exe"
  )
  foreach ($p in $paths) {
    if (Test-Path $p) { $clangFormat = $p; break }
  }
  if (-not $clangFormat) {
    $vsPath = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "clang-format.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    if ($vsPath) { $clangFormat = $vsPath }
  }
}
if (-not $clangFormat) {
  Write-Error "clang-format not found. Install LLVM (e.g. from https://releases.llvm.org/) and add its bin folder to PATH, or place clang-format.exe in a folder on PATH."
  exit 1
}

Get-ChildItem -Path "XboxHomebrewStore" -Recurse -Include *.cpp,*.h,*.c | Where-Object {
  $n = $_.Name
  ($n -notlike "stb*") -and ($n -ne "ssfn.h") -and ($n -ne "parson.h") -and ($n -ne "parson.c")
} | ForEach-Object { & $clangFormat -i $_.FullName }
