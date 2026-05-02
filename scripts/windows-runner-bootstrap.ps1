# Prepares a fresh Windows x64 VM as Snim's self-hosted GitHub Actions runner.
# Run from an elevated Windows PowerShell 5.1 (or pwsh) prompt:
#   .\windows-runner-bootstrap.ps1 -DryRun                   # show what would happen
#   .\windows-runner-bootstrap.ps1                           # toolchain, cache dir, service user
#   .\windows-runner-bootstrap.ps1 -RunnerToken <token>      # ...and register the runner service
# The token comes from github.com/organizations/snimdev/settings/actions/runners/new and expires in an hour.
# Safe to re-run: every step skips what is already in place.

[CmdletBinding()]
param(
    [string]$RunnerToken,
    [string]$RunnerUrl = 'https://github.com/snimdev',
    [string]$Labels = 'snim-win',
    [string]$RunnerDir = 'C:\actions-runner',
    [string]$ServiceUser = 'snim-runner',
    [string]$CacheRoot = 'C:\snim-ci',
    [switch]$DefenderExclusions,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Write-Step([string]$Message) { Write-Host "==> $Message" -ForegroundColor Cyan }
function Write-Skip([string]$Message) { Write-Host "    ok: $Message" -ForegroundColor DarkGray }

# Runs $Action unless -DryRun, in which case it only says what it would do.
function Invoke-Action([string]$Description, [scriptblock]$Action) {
    if ($DryRun) {
        Write-Host "    [dry run] $Description" -ForegroundColor Yellow
        return
    }
    Write-Host "    $Description"
    & $Action
}

function Update-SessionPath {
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = "$machine;$user"
}

function Test-WingetPackage([string]$Id) {
    & winget list --exact --id $Id --accept-source-agreements --disable-interactivity *> $null
    return $LASTEXITCODE -eq 0
}

function Install-WingetPackage([string]$Id, [string]$Override) {
    if (Test-WingetPackage $Id) {
        Write-Skip "$Id is installed"
        return
    }
    $wingetArgs = @('install', '--exact', '--id', $Id, '--scope', 'machine', '--silent',
        '--accept-source-agreements', '--accept-package-agreements', '--disable-interactivity')
    if ($Override) { $wingetArgs += @('--override', $Override) }
    Invoke-Action "winget $($wingetArgs -join ' ')" {
        & winget @wingetArgs
        if ($LASTEXITCODE -ne 0) { throw "winget failed to install $Id (exit $LASTEXITCODE)" }
    }
}

function Get-VsWhere {
    $path = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $path) { return $path }
    return $null
}

function Test-VcTools {
    $vswhere = Get-VsWhere
    if (-not $vswhere) { return $false }
    $found = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    return [bool]$found
}

# Preflight

Write-Step 'Checking the machine'
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    if ($DryRun) { Write-Host '    not elevated: fine for a dry run, required for the real one' -ForegroundColor Yellow }
    else { throw 'Run this from an elevated PowerShell prompt (Run as administrator).' }
}
# 9 is x64; an ARM64 VM would report 12 even to an emulated x64 PowerShell.
$arch = (Get-CimInstance Win32_Processor | Select-Object -First 1).Architecture
if (-not [Environment]::Is64BitOperatingSystem -or $arch -ne 9) {
    throw "Snim's Windows runner must be x64 (processor architecture code $arch)."
}
Write-Skip "x64, $((Get-CimInstance Win32_OperatingSystem).Caption)"
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw 'winget is missing. Windows Server 2022 does not ship it: install App Installer (Microsoft.DesktopAppInstaller) first, or use Windows 11 / Server 2025.'
}
Write-Skip 'winget is available'

# Toolchain

Write-Step 'Installing the toolchain'
Install-WingetPackage 'Git.Git'
# The runner's default shell is pwsh; without it, steps fall back to Windows PowerShell 5.1.
# The MSI directly: winget can reject every installer in the package for --scope machine.
$pwsh = "$env:ProgramFiles\PowerShell\7\pwsh.exe"
if (Test-Path $pwsh) {
    Write-Skip 'PowerShell 7 is installed'
} else {
    Invoke-Action 'install PowerShell 7 from its MSI' {
        $release = Invoke-RestMethod -UseBasicParsing 'https://api.github.com/repos/PowerShell/PowerShell/releases/latest'
        $asset = $release.assets | Where-Object { $_.name -like '*-win-x64.msi' } | Select-Object -First 1
        $msi = Join-Path $env:TEMP $asset.name
        Invoke-WebRequest -UseBasicParsing $asset.browser_download_url -OutFile $msi
        $p = Start-Process msiexec -ArgumentList "/i `"$msi`" /quiet ADD_PATH=1" -Wait -PassThru
        if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { throw "PowerShell 7 MSI exited with $($p.ExitCode)" }
    }
}
# Machine-wide and on PATH: install-qt-action runs aqtinstall with whatever python the service sees.
Install-WingetPackage 'Python.Python.3.14' '/quiet InstallAllUsers=1 PrependPath=1 Include_test=0'
Install-WingetPackage '7zip.7zip'
Install-WingetPackage 'JRSoftware.InnoSetup'

$vsArgs = '--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
if (Test-VcTools) {
    Write-Skip 'Visual Studio 2022 Build Tools with the C++ workload is installed'
} elseif (Test-WingetPackage 'Microsoft.VisualStudio.2022.BuildTools') {
    # Build Tools without the C++ workload: winget would call it installed, so modify it instead.
    $vsPath = & (Get-VsWhere) -products * -property installationPath | Select-Object -First 1
    $setup = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"
    Invoke-Action "add the C++ workload to $vsPath" {
        $p = Start-Process $setup -ArgumentList "modify --installPath `"$vsPath`" $vsArgs" -Wait -PassThru
        if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { throw "Visual Studio Installer exited with $($p.ExitCode)" }
    }
} else {
    Install-WingetPackage 'Microsoft.VisualStudio.2022.BuildTools' $vsArgs
}

Update-SessionPath
# Git\bin for bash.exe: composite actions such as install-qt-action run `shell: bash` steps.
foreach ($dir in @("$env:ProgramFiles\7-Zip", "$env:ProgramFiles\Git\bin")) {
    $machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    if (($machinePath -split ';') -contains $dir) {
        Write-Skip "$dir is on the machine PATH"
    } else {
        Invoke-Action "add $dir to the machine PATH" {
            [Environment]::SetEnvironmentVariable('Path', "$machinePath;$dir", 'Machine')
            Update-SessionPath
        }
    }
}

# Long paths

Write-Step 'Enabling long paths'
$fsKey = 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'
if ((Get-ItemProperty $fsKey -Name LongPathsEnabled -ErrorAction SilentlyContinue).LongPathsEnabled -eq 1) {
    Write-Skip 'Win32 long paths are enabled'
} else {
    Invoke-Action "set $fsKey\LongPathsEnabled = 1" {
        Set-ItemProperty $fsKey -Name LongPathsEnabled -Value 1 -Type DWord
    }
}
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git -and $DryRun) {
    Write-Host '    [dry run] git config --system core.longpaths true' -ForegroundColor Yellow
} elseif (-not $git) {
    throw 'git is not on PATH after installing Git.Git; open a new elevated prompt and re-run.'
} elseif ((& git config --system --get core.longpaths) -eq 'true') {
    Write-Skip 'git core.longpaths is set system-wide'
} else {
    Invoke-Action 'git config --system core.longpaths true' {
        & git config --system core.longpaths true
        if ($LASTEXITCODE -ne 0) { throw "git config failed (exit $LASTEXITCODE)" }
    }
}

# Service user

Write-Step "Preparing the service user $ServiceUser"
$password = $null
$userExists = [bool](Get-LocalUser -Name $ServiceUser -ErrorAction SilentlyContinue)
if ($userExists) {
    Write-Skip "local user $ServiceUser exists"
} elseif ($DryRun) {
    Write-Host "    [dry run] prompt for a password and create local user $ServiceUser (member of Users)" -ForegroundColor Yellow
} else {
    $password = Read-Host "Password for the new local user $ServiceUser" -AsSecureString
    Write-Host "    create local user $ServiceUser"
    New-LocalUser -Name $ServiceUser -Password $password -PasswordNeverExpires -AccountNeverExpires `
        -Description 'Snim GitHub Actions runner service' | Out-Null
    Add-LocalGroupMember -Group 'Users' -Member $ServiceUser -ErrorAction SilentlyContinue
}

# Persistent cache

Write-Step "Creating the persistent cache under $CacheRoot"
foreach ($dir in @($CacheRoot, "$CacheRoot\vcpkg-cache", "$CacheRoot\vcpkg-downloads", "$CacheRoot\qt")) {
    if (Test-Path $dir) { Write-Skip "$dir exists" }
    else { Invoke-Action "create $dir" { New-Item -ItemType Directory -Force $dir | Out-Null } }
}
Invoke-Action "grant $ServiceUser modify rights on $CacheRoot" {
    & icacls $CacheRoot /grant "${ServiceUser}:(OI)(CI)M" /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "icacls failed (exit $LASTEXITCODE)" }
}

# Defender

if ($DefenderExclusions) {
    # Real-time scanning of every object file, vcpkg archive and Qt DLL the build writes costs a large share of its time.
    Write-Step 'Adding Defender exclusions'
    $existing = @((Get-MpPreference).ExclusionPath)
    foreach ($dir in @($CacheRoot, $RunnerDir)) {
        if ($existing -contains $dir) { Write-Skip "$dir is excluded" }
        else { Invoke-Action "exclude $dir from Defender scanning" { Add-MpPreference -ExclusionPath $dir } }
    }
}

# Runner

if ($RunnerToken) {
    Write-Step "Registering the runner in $RunnerDir"
    if (Test-Path "$RunnerDir\.runner") {
        Write-Skip "$RunnerDir is already configured; run config.cmd remove there first to re-register"
    } else {
        $release = Invoke-RestMethod -UseBasicParsing 'https://api.github.com/repos/actions/runner/releases/latest'
        $asset = $release.assets | Where-Object { $_.name -like 'actions-runner-win-x64-*.zip' } | Select-Object -First 1
        if (-not $asset) { throw "Release $($release.tag_name) has no win-x64 zip." }

        $expected = @()
        if ($asset.digest -match '^sha256:([0-9a-fA-F]{64})$') { $expected += $Matches[1].ToLower() }
        if ($release.body -match '<!-- BEGIN SHA win-x64 -->([0-9a-fA-F]{64})<!-- END SHA win-x64 -->') { $expected += $Matches[1].ToLower() }
        $expected = @($expected | Select-Object -Unique)
        if ($expected.Count -eq 0) { throw "Release $($release.tag_name) lists no SHA-256 for $($asset.name); refusing to install it unverified." }
        if ($expected.Count -gt 1) { throw "Release $($release.tag_name) lists conflicting SHA-256 values for $($asset.name)." }
        Write-Skip "latest runner is $($release.tag_name), expected SHA-256 $($expected[0])"

        $zip = Join-Path $env:TEMP $asset.name
        Invoke-Action "download $($asset.browser_download_url), verify it and extract it to $RunnerDir" {
            Invoke-WebRequest -UseBasicParsing $asset.browser_download_url -OutFile $zip
            $actual = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
            if ($actual -ne $expected[0]) {
                Remove-Item $zip -Force
                throw "SHA-256 mismatch for $($asset.name): expected $($expected[0]), got $actual"
            }
            New-Item -ItemType Directory -Force $RunnerDir | Out-Null
            Expand-Archive $zip -DestinationPath $RunnerDir -Force
            Remove-Item $zip -Force
        }

        if (-not $password -and -not $DryRun) {
            $password = Read-Host "Password of the existing local user $ServiceUser" -AsSecureString
        }
        $configArgs = @('--unattended', '--url', $RunnerUrl, '--token', '<token>', '--labels', $Labels,
            '--name', $env:COMPUTERNAME, '--runasservice', '--windowslogonaccount', ".\$ServiceUser",
            '--windowslogonpassword', '<password>', '--work', '_work', '--replace')
        Invoke-Action "config.cmd $($configArgs -join ' ')" {
            $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($password)
            try {
                $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
                $realArgs = $configArgs.Clone()
                $realArgs[[array]::IndexOf($realArgs, '<token>')] = $RunnerToken
                $realArgs[[array]::IndexOf($realArgs, '<password>')] = $plain
                Push-Location $RunnerDir
                try { & .\config.cmd @realArgs } finally { Pop-Location }
                if ($LASTEXITCODE -ne 0) { throw "config.cmd failed (exit $LASTEXITCODE)" }
            } finally {
                [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
                $plain = $null
            }
        }
    }
} else {
    Write-Step 'Skipping runner registration (no -RunnerToken)'
}

Write-Step 'Next steps'
Write-Host @"
  1. Reboot once if Build Tools or long paths were just installed, then check the runner shows Idle under
     github.com/organizations/snimdev/settings/actions/runners (runner group must allow the snim repo).
  2. In github.com/snimdev/snim/settings/variables/actions set the repository variable
       WINDOWS_RUNNER = ["self-hosted","windows","x64","snim-win"]
  3. Re-run the Windows checks on the open pull request. The first build is cold (vcpkg builds every
     dependency into $CacheRoot\vcpkg-cache); later ones reuse it.
  To move the caches, set SNIM_VCPKG_CACHE and SNIM_VCPKG_DOWNLOADS in $RunnerDir\.env and restart the runner service.
"@
