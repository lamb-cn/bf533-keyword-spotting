param(
 [string]$Cces = 'C:\Analog Devices\CrossCore Embedded Studio 2.10.0',
 [switch]$BuildOnly
)
& (Join-Path $PSScriptRoot 'run_recognize.ps1') -Cces $Cces -Listen -BuildOnly:$BuildOnly
