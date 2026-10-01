#Requires -Version 7
param(
 [string]$BaseClasses='C:\tmp\potplayer-gpupixel-bridge\third_party\BaseClasses',
 [string]$LibYuvSource='C:\tmp\potplayer-gpupixel-bridge\third_party\gpupixel\third_party\libyuv',
 [string]$LibYuvLibrary='C:\tmp\potplayer-gpupixel-bridge\build\libyuv-clang\yuv.lib'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'output/local-video/build'
New-Item -ItemType Directory -Force $out | Out-Null
if(!(Test-Path (Join-Path $BaseClasses 'streams.h'))){throw 'DirectShow BaseClasses source is required. Pass -BaseClasses.'}
if(!(Test-Path (Join-Path $LibYuvSource 'include/libyuv/convert_argb.h')) -or !(Test-Path $LibYuvLibrary)){throw 'libyuv headers and a static x64 library are required. Pass -LibYuvSource and -LibYuvLibrary.'}
$sources=Get-ChildItem -LiteralPath $BaseClasses -Filter *.cpp | Where-Object Name -ne 'sysclock.cpp'
$items=($sources | ForEach-Object {'<ClCompile Include="'+[Security.SecurityElement]::Escape($_.FullName)+'" />'}) -join "`n"
$template=@'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
 <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
 <PropertyGroup Label="Globals"><ProjectGuid>{671BF4B2-6DA8-4F3B-A174-7C8937C561C2}</ProjectGuid><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
 <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
 <PropertyGroup Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><PlatformToolset>v143</PlatformToolset><CharacterSet>Unicode</CharacterSet></PropertyGroup>
 <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
 <PropertyGroup><OutDir>$(ProjectDir)bin\</OutDir><IntDir>$(ProjectDir)obj\</IntDir><TargetName>MagpieLocalVideoTap</TargetName><TargetExt>.ax</TargetExt></PropertyGroup>
 <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp17</LanguageStandard><SDLCheck>false</SDLCheck><ExceptionHandling>Sync</ExceptionHandling><ConformanceMode>false</ConformanceMode><WarningLevel>Level3</WarningLevel><Optimization>MaxSpeed</Optimization><PreprocessorDefinitions>WIN32;_WINDOWS;_USRDLL;UNICODE;_UNICODE;VFWROBUST;NDEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions><AdditionalIncludeDirectories>__BASE__;C:\tmp\potplayer-gpupixel-bridge\third_party\gpupixel\third_party\libyuv\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><DisableSpecificWarnings>4996;%(DisableSpecificWarnings)</DisableSpecificWarnings></ClCompile>
 <Link><ModuleDefinitionFile>__ROOT__\src\LocalVideoTap\LocalVideoTap.def</ModuleDefinitionFile><AdditionalDependencies>C:\tmp\potplayer-gpupixel-bridge\build\libyuv-clang\yuv.lib;strmiids.lib;quartz.lib;winmm.lib;msvfw32.lib;ole32.lib;oleaut32.lib;uuid.lib;legacy_stdio_definitions.lib;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup>
 <ItemGroup><ClCompile Include="__ROOT__\src\LocalVideoTap\LocalVideoTap.cpp" />__SOURCES__</ItemGroup>
 <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
'@
$template=$template.Replace('C:\tmp\potplayer-gpupixel-bridge\third_party\gpupixel\third_party\libyuv\include',[Security.SecurityElement]::Escape((Join-Path $LibYuvSource 'include'))).Replace('C:\tmp\potplayer-gpupixel-bridge\build\libyuv-clang\yuv.lib',[Security.SecurityElement]::Escape($LibYuvLibrary))
$template.Replace('__ROOT__',[Security.SecurityElement]::Escape($root)).Replace('__BASE__',[Security.SecurityElement]::Escape($BaseClasses)).Replace('__SOURCES__',$items) | Set-Content (Join-Path $out 'LocalVideoTap.vcxproj') -Encoding utf8
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1)
$msbuild=Join-Path $vs 'MSBuild/Current/Bin/amd64/MSBuild.exe'
$psi=[Diagnostics.ProcessStartInfo]::new($msbuild)
foreach($a in @((Join-Path $out 'LocalVideoTap.vcxproj'),'/m:2','/nr:false','/v:minimal','/p:Configuration=Release','/p:Platform=x64')){[void]$psi.ArgumentList.Add($a)}
$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
$value=$psi.Environment['PATH'];[void]$psi.Environment.Remove('PATH');$psi.Environment['Path']=$value
$p=[Diagnostics.Process]::Start($psi);$o=$p.StandardOutput.ReadToEndAsync();$e=$p.StandardError.ReadToEndAsync();$p.WaitForExit()
$o.Result+$e.Result | Set-Content (Join-Path $out 'build.log')
if($p.ExitCode){Get-Content (Join-Path $out 'build.log') -Tail 35;throw 'Local video tap build failed'}
$env:DOTNET_CLI_HOME=Join-Path $out 'dotnet-home'
dotnet publish (Join-Path $root 'src/LocalVideoHost/LocalVideoHost.csproj') -c Release -r win-x64 --self-contained false -o (Join-Path $out 'bin') --nologo
if($LASTEXITCODE){throw 'Local video host build failed'}
Copy-Item -LiteralPath (Join-Path $LibYuvSource 'LICENSE') -Destination (Join-Path $out 'bin/libyuv-LICENSE.txt') -Force
Get-ChildItem (Join-Path $out 'bin') -File | Select-Object Name,Length
