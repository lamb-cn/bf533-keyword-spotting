param(
 [string]$Cces = 'C:\Analog Devices\CrossCore Embedded Studio 2.10.0',
 [string]$Python,
 [switch]$BuildOnly
)
$pythonOption = @{}
if ($Python) { $pythonOption.Python = $Python }
& (Join-Path $PSScriptRoot 'run_recognize.ps1') -Cces $Cces -Listen -BuildOnly:$BuildOnly @pythonOption
