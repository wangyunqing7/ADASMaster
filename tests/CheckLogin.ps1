$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $workspace 'qml\Main.qml'
$loginPath = Join-Path $workspace 'qml\pages\LoginView.qml'
$cmakePath = Join-Path $workspace 'CMakeLists.txt'

$main = Get-Content -Raw -Encoding UTF8 $mainPath
$cmake = Get-Content -Raw -Encoding UTF8 $cmakePath

if (-not (Test-Path -LiteralPath $loginPath -PathType Leaf)) {
    throw 'LoginView.qml does not exist.'
}

if ($main -notmatch 'LoginView\s*\{') {
    throw 'Main.qml does not place LoginView.'
}
if ($main -notmatch 'authManager\.isLoggedIn') {
    throw 'Main.qml must switch on authManager.isLoggedIn.'
}

$login = Get-Content -Raw -Encoding UTF8 $loginPath
# LoginView 通过 auth 别名(appController.authManager)访问，故匹配 auth.* 而非 authManager.*
$requiredBindings = @('authManager','auth.login','auth.loginError','echoMode: TextInput.Password',
                      'TextField','Button')
foreach ($binding in $requiredBindings) {
    if ($login -notmatch [regex]::Escape($binding)) {
        throw "LoginView.qml is missing '$binding'."
    }
}

if ($cmake -notmatch 'qml/pages/LoginView\.qml') {
    throw 'LoginView.qml is not registered in qt_add_qml_module.'
}
if ($cmake -notmatch 'qml/icons/logout\.svg') {
    throw 'logout.svg is not registered in RESOURCES.'
}

$topbar = Get-Content -Raw -Encoding UTF8 (Join-Path $workspace 'qml\components\TopStatusBar.qml')
if ($topbar -notmatch 'authManager\.logout') {
    throw 'TopStatusBar.qml must call authManager.logout for the logout button.'
}
$settings = Get-Content -Raw -Encoding UTF8 (Join-Path $workspace 'qml\pages\SettingsPage.qml')
if ($settings -notmatch 'auth\.currentRole') {
    throw 'SettingsPage.qml must bind permissions to auth.currentRole.'
}

Write-Host 'PASS: login screen and logout are integrated.'
