#!/usr/bin/env bash
set -euo pipefail

# Provision a Windows Server 2022 VM on Azure and enable SSH with your public key.
# Requires: az CLI authenticated with sufficient permissions.
# Usage:
#   SUBSCRIPTION_ID="..." LOCATION="australiaeast" ./provision-windows-azure.sh

SUBSCRIPTION_ID="${SUBSCRIPTION_ID:-}"
RESOURCE_GROUP="${RESOURCE_GROUP:-extempore-win}"
LOCATION="${LOCATION:-}"
VM_NAME="${VM_NAME:-extempore-winvm}"
VM_SIZE="${VM_SIZE:-Standard_D4s_v5}"
ADMIN_USER="${ADMIN_USER:-azureuser}"
SSH_PUBKEY_PATH="${SSH_PUBKEY_PATH:-$HOME/.ssh/id_ed25519.pub}"

if [ -z "$SUBSCRIPTION_ID" ]; then
  echo "SUBSCRIPTION_ID is required. Export it before running." >&2
  exit 1
fi

if [ -z "$LOCATION" ]; then
  echo "LOCATION is required (for example: australiaeast)." >&2
  exit 1
fi

if [ ! -f "$SSH_PUBKEY_PATH" ]; then
  echo "SSH public key not found at $SSH_PUBKEY_PATH" >&2
  exit 1
fi

if ! command -v az >/dev/null 2>&1; then
  echo "az CLI not found. Install Azure CLI first." >&2
  exit 1
fi

read -r -s -p "Admin password for $ADMIN_USER (will be used only for VM creation): " ADMIN_PASSWORD
printf "\n"

if [ -z "$ADMIN_PASSWORD" ]; then
  echo "Password cannot be empty." >&2
  exit 1
fi

az account set --subscription "$SUBSCRIPTION_ID"

# Create resource group if needed.
az group create -n "$RESOURCE_GROUP" -l "$LOCATION" >/dev/null

# Create the VM with password auth (SSH for Windows is enabled post-provisioning).
if az vm show -g "$RESOURCE_GROUP" -n "$VM_NAME" >/dev/null 2>&1; then
  echo "VM $VM_NAME already exists; skipping create."
else
  az vm create \
    -g "$RESOURCE_GROUP" \
    -n "$VM_NAME" \
    --image MicrosoftWindowsServer:WindowsServer:2022-datacenter-g2:latest \
    --size "$VM_SIZE" \
    --admin-username "$ADMIN_USER" \
    --admin-password "$ADMIN_PASSWORD" \
    --public-ip-sku Standard
fi

# Open SSH port.
az vm open-port -g "$RESOURCE_GROUP" -n "$VM_NAME" --port 22 >/dev/null

# Install and enable OpenSSH server, set firewall, and add the authorized key.
PUBKEY_CONTENT=$(cat "$SSH_PUBKEY_PATH")

PS_TEMPLATE=$(cat <<'PS1'
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
Start-Service sshd
Set-Service -Name sshd -StartupType 'Automatic'
if (-not (Get-NetFirewallRule -Name sshd -ErrorAction SilentlyContinue)) { New-NetFirewallRule -Name sshd -DisplayName 'OpenSSH Server (sshd)' -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 }
$pubkey = @'
__PUBKEY_CONTENT__
'@
New-Item -ItemType Directory -Force -Path C:\Users\__ADMIN_USER__\.ssh | Out-Null
Set-Content -Path C:\Users\__ADMIN_USER__\.ssh\authorized_keys -Value $pubkey
icacls C:\Users\__ADMIN_USER__\.ssh /inheritance:r /grant __ADMIN_USER__:(F) | Out-Null
icacls C:\Users\__ADMIN_USER__\.ssh\authorized_keys /inheritance:r /grant __ADMIN_USER__:(F) | Out-Null
PS1
)

PS_SCRIPT=${PS_TEMPLATE//__ADMIN_USER__/$ADMIN_USER}
PS_SCRIPT=${PS_SCRIPT//__PUBKEY_CONTENT__/$PUBKEY_CONTENT}

az vm run-command invoke \
  -g "$RESOURCE_GROUP" \
  -n "$VM_NAME" \
  --command-id RunPowerShellScript \
  --scripts "$PS_SCRIPT"

PUBLIC_IP=$(az vm show -d -g "$RESOURCE_GROUP" -n "$VM_NAME" --query publicIps -o tsv)

echo "VM created. SSH in with:"
echo "ssh $ADMIN_USER@$PUBLIC_IP"
