<#
.SYNOPSIS
    Reconfigure ESP-IDF project with a specific board configuration.

.DESCRIPTION
    Deletes sdkconfig and runs idf.py reconfigure with the specified board defaults.

.PARAMETER Board
    The board suffix (e.g., "esp32s3_ili9341" or "esp32s3_rgb")

.EXAMPLE
    .\reconfig.ps1 esp32s3_ili9341
    .\reconfig.ps1 esp32s3_rgb
#>

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Board
)

$sdkconfig = "sdkconfig"
$defaults = "sdkconfig.defaults;sdkconfig.defaults.$Board"

# Delete sdkconfig if it exists
if (Test-Path $sdkconfig) {
    Remove-Item $sdkconfig -Force
    Write-Host "Deleted $sdkconfig" -ForegroundColor Yellow
}

# Run idf.py reconfigure
Write-Host "Running: idf.py -D SDKCONFIG_DEFAULTS=`"$defaults`" reconfigure" -ForegroundColor Cyan
idf.py -D SDKCONFIG_DEFAULTS="$defaults" reconfigure
