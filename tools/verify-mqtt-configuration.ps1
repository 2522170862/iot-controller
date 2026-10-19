param(
  [string]$Compiler = 'g++',
  [switch]$Zig,
  [string]$ArduinoJsonInclude = "$env:USERPROFILE\Documents\Arduino\libraries\ArduinoJson\src"
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('iot-mqtt-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory | Out-Null
Push-Location $repository
try {
  $common = @('-std=c++17', '-Wall', '-Wextra', '-I', $ArduinoJsonInclude)
  if ($Zig) { $common = @('c++', '-target', 'x86_64-windows-gnu') + $common }
  $tests = @(
    'tests/mqtt_provisioning_test/mqtt_provisioning_test.cpp',
    'tests/mqtt_provisioning_test/mqtt_service_configuration_test.cpp',
    'tests/ble_provisioning_protocol_test/ble_provisioning_protocol_test.ino',
    'tests/wifi_provisioning_coordinator_test/wifi_provisioning_coordinator_test.ino',
    'tests/wifi_connection_retry_test/wifi_connection_retry_test.cpp'
  )
  foreach ($source in $tests) {
    $output = Join-Path $testDirectory (([System.IO.Path]::GetFileNameWithoutExtension($source)) + '.exe')
    $arguments = $common + @('-I', 'tests/mqtt_provisioning_test')
    if ($source -like 'tests/wifi_connection_retry_test/*') {
      $arguments = $common + @('-I', 'tests/wifi_connection_retry_test')
    }
    if ($source.EndsWith('.ino')) { $arguments += @('-x', 'c++', '-include', 'tests/mqtt_provisioning_test/arduino_test_main.h') }
    $arguments += @($source, '-o', $output)
    & $Compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    & $output
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $source" }
  }
} finally { Pop-Location }
