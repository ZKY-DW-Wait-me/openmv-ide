# OpenMV IDE Native Windows One-Click Build & Packaging Script
# Requires Qt 6 (e.g. Qt 6.5.x MinGW 64-bit), CMake, Ninja, and Qt Installer Framework (QtIFW)

param (
    [string]$QtRoot = "C:\Qt",
    [switch]$SkipInstaller = $false,
    [switch]$ViewerMode = $false
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   OpenMV IDE Native Windows Build & Packaging Pipeline   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Detect Qt directory
$qtDir = $null
if (Test-Path $QtRoot) {
    # Search for Qt 6.x mingw_64
    $qtVersions = Get-ChildItem -Path $QtRoot -Directory | Where-Object { $_.Name -match '^\d+\.\d+' } | Sort-Object Name -Descending
    foreach ($ver in $qtVersions) {
        $mingwPath = Get-ChildItem -Path $ver.FullName -Directory | Where-Object { $_.Name -match 'mingw' } | Select-Object -First 1
        if ($mingwPath) {
            $qtDir = $mingwPath.FullName
            break
        }
    }
}

if (-not $qtDir) {
    Write-Host "[WARNING] Qt 6 MinGW directory not automatically found at $QtRoot." -ForegroundColor Yellow
    Write-Host "Please ensure Qt 6.5+ Desktop MinGW 64-bit is installed via Qt Online Installer to C:\Qt." -ForegroundColor Yellow
    Write-Host "You can also specify the path manually: .\build_native_windows.ps1 -QtRoot 'D:\Qt'" -ForegroundColor Yellow
} else {
    Write-Host "[OK] Found Qt SDK at: $qtDir" -ForegroundColor Green
    $env:QTDIR = $qtDir
    $env:PATH = "$qtDir\bin;" + $env:PATH
}

# 2. Detect MinGW Compiler Toolchain
$toolsDir = Join-Path $QtRoot "Tools"
if (Test-Path $toolsDir) {
    $mingwTool = Get-ChildItem -Path $toolsDir -Directory | Where-Object { $_.Name -match 'mingw' } | Select-Object -First 1
    if ($mingwTool) {
        $env:MINGWDIR = $mingwTool.FullName
        $env:PATH = "$($mingwTool.FullName)\bin;" + $env:PATH
        Write-Host "[OK] Found MinGW compiler at: $($mingwTool.FullName)" -ForegroundColor Green
    }
    
    $cmakeTool = Get-ChildItem -Path $toolsDir -Directory | Where-Object { $_.Name -match 'CMake' } | Select-Object -First 1
    if ($cmakeTool) {
        $env:CMAKEDIR = $cmakeTool.FullName
        $env:PATH = "$($cmakeTool.FullName)\bin;" + $env:PATH
        Write-Host "[OK] Found CMake at: $($cmakeTool.FullName)" -ForegroundColor Green
    }

    $ninjaTool = Get-ChildItem -Path $toolsDir -Directory | Where-Object { $_.Name -match 'Ninja' } | Select-Object -First 1
    if ($ninjaTool) {
        $env:NINJADIR = $ninjaTool.FullName
        $env:PATH = "$($ninjaTool.FullName);" + $env:PATH
        Write-Host "[OK] Found Ninja at: $($ninjaTool.FullName)" -ForegroundColor Green
    }

    $ifwTool = Get-ChildItem -Path $toolsDir -Directory | Where-Object { $_.Name -match 'QtInstallerFramework' } | Select-Object -First 1
    if ($ifwTool) {
        $ifwVer = Get-ChildItem -Path $ifwTool.FullName -Directory | Select-Object -First 1
        if ($ifwVer) {
            $env:IFDIR = $ifwVer.FullName
            $env:PATH = "$($ifwVer.FullName)\bin;" + $env:PATH
            Write-Host "[OK] Found QtIFW at: $($ifwVer.FullName)" -ForegroundColor Green
        }
    }
}

# 3. Assemble arguments for make.py
$makeArgs = @("make.py", "--no-sign-application", "--no-sign-installer")
if ($SkipInstaller) {
    $makeArgs += "--no-build-installer"
}
if ($ViewerMode) {
    $makeArgs += "--viewer"
}

Write-Host "Running: python $($makeArgs -join ' ')" -ForegroundColor Cyan
& python $makeArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "==========================================================" -ForegroundColor Green
    Write-Host "   OpenMV IDE Built & Packaged Successfully!              " -ForegroundColor Green
    Write-Host "   Output packages are located in: build\                 " -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green
} else {
    Write-Host "[ERROR] Build failed with exit code $LASTEXITCODE." -ForegroundColor Red
    exit $LASTEXITCODE
}
