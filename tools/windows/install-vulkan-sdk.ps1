# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

# Check if running as administrator
try {
    net session 1>$null 2>$null
} catch {
    Write-Host "This script must be run with administrator privileges!"
    Exit 1
}

$VulkanSDKVer = "1.4.341.1"
if ((Get-WmiObject -Class Win32_ComputerSystem).SystemType -match '(x64)' -eq "True") {
    $VulkanSDKArch = "X64"
    $VulkanSDKOs = "windows"
} else {
    $VulkanSDKArch = "ARM64"
    $VulkanSDKOs = "warm"
}
$VULKAN_SDK = "C:/VulkanSDK/$VulkanSDKVer"
$ExeFile = "vulkansdk-windows-$VulkanSDKArch-$VulkanSDKVer.exe"
$Uri = "https://sdk.lunarg.com/sdk/download/$VulkanSDKVer/$VulkanSDKOs/$ExeFile"
$Destination = "./$ExeFile"

# Check if Vulkan SDK is already installed
if (Test-Path $VULKAN_SDK) {
    Write-Host "-- Vulkan SDK already installed at $VULKAN_SDK"
    return
}

echo "Downloading Vulkan SDK $VulkanSDKVer from $Uri"
for ($Attempt = 1; $Attempt -le 3; $Attempt++) {
    try {
        Invoke-WebRequest -Uri $Uri -OutFile $Destination -TimeoutSec 300 -UseBasicParsing
        break
    } catch {
        Write-Host "Download attempt $Attempt failed: $_"
        if ($Attempt -eq 3) { throw }
        Start-Sleep -Seconds 10
    }
}
echo "Finished downloading $ExeFile"

$Arguments = "--root `"$VULKAN_SDK`" --accept-licenses --default-answer --confirm-command install"

echo "Installing Vulkan SDK $VulkanSDKVer"
$InstallProcess = Start-Process -FilePath $Destination -NoNewWindow -PassThru -ArgumentList $Arguments
if (-not $InstallProcess.WaitForExit(600000)) {
    echo "Error: Vulkan SDK installer timed out"
    Stop-Process -Id $InstallProcess.Id -Force -ErrorAction SilentlyContinue
    Exit 1
}
$ExitCode = $InstallProcess.ExitCode

if ($ExitCode -ne 0) {
    echo "Error installing Vulkan SDK $VulkanSDKVer (Error: $ExitCode)"
    Exit $ExitCode
}

echo "Finished installing Vulkan SDK $VulkanSDKVer"

if ("$env:GITHUB_ACTIONS" -eq "true") {
    echo "VULKAN_SDK=$VULKAN_SDK" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    echo "$VULKAN_SDK/Bin" | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
}
