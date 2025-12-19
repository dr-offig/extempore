# Bootstrap a Windows Server 2022 VM for Extempore builds.
# Run in an elevated PowerShell session.

Set-ExecutionPolicy Bypass -Scope Process -Force
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072

# Install Chocolatey if missing.
if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
  iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
}

# Core build tooling + Node LTS.
choco install -y git cmake ninja python 7zip nodejs-lts
choco install -y visualstudio2022buildtools visualstudio2022-workload-vctools

# Refresh PATH for this session.
if (Test-Path "$env:ChocolateyInstall\helpers\chocolateyProfile.psm1") {
  Import-Module "$env:ChocolateyInstall\helpers\chocolateyProfile.psm1"
  refreshenv
}

# Install Claude Code.
npm install -g @anthropic-ai/claude-code

# Enable OpenSSH server and firewall rule.
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
Start-Service sshd
Set-Service -Name sshd -StartupType 'Automatic'
if (-not (Get-NetFirewallRule -Name sshd -ErrorAction SilentlyContinue)) {
  New-NetFirewallRule -Name sshd -DisplayName 'OpenSSH Server (sshd)' -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22
}

# Clone repo.
$RepoUrl = 'https://github.com/extemporelang/extempore.git'
$RepoRoot = 'C:\src'
if (-not (Test-Path $RepoRoot)) {
  New-Item -ItemType Directory -Path $RepoRoot | Out-Null
}
Set-Location $RepoRoot
if (-not (Test-Path (Join-Path $RepoRoot 'extempore'))) {
  git clone $RepoUrl
}

Write-Host 'Bootstrap complete. Next steps:'
Write-Host '1) claude auth login'
Write-Host '2) Open "x64 Native Tools Command Prompt for VS 2022"'
Write-Host '3) cd C:\src\extempore && mkdir build && cd build'
Write-Host '4) cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release'
Write-Host '5) cmake --build . -j 8'
