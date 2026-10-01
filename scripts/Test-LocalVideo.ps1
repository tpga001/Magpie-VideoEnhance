#Requires -Version 7
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'output/local-video/unit'
New-Item -ItemType Directory -Force $out | Out-Null
$project=@'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
 <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
 <PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
 <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
 <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup>
 <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
 <PropertyGroup><OutDir>$(ProjectDir)</OutDir><IntDir>$(ProjectDir)obj\</IntDir><TargetName>LocalVideoFrameTests</TargetName></PropertyGroup>
 <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp17</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling><UndefinePreprocessorDefinitions>NDEBUG;%(UndefinePreprocessorDefinitions)</UndefinePreprocessorDefinitions></ClCompile></ItemDefinitionGroup>
 <ItemGroup><ClCompile Include="__ROOT__\tests\LocalVideoFrameTests.cpp" /></ItemGroup>
 <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
'@
$path=Join-Path $out 'LocalVideoFrameTests.vcxproj'
$project.Replace('__ROOT__',[Security.SecurityElement]::Escape($root)) | Set-Content $path -Encoding utf8
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1)
$psi=[Diagnostics.ProcessStartInfo]::new((Join-Path $vs 'MSBuild/Current/Bin/amd64/MSBuild.exe'))
foreach($a in @($path,'/nr:false','/v:minimal','/p:Configuration=Release','/p:Platform=x64')){[void]$psi.ArgumentList.Add($a)}
$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true
$value=$psi.Environment['PATH'];[void]$psi.Environment.Remove('PATH');$psi.Environment['Path']=$value
$p=[Diagnostics.Process]::Start($psi);$p.WaitForExit();if($p.ExitCode){throw 'Local video unit test build failed'}
& (Join-Path $out 'LocalVideoFrameTests.exe')
if($LASTEXITCODE){throw 'Local video unit tests failed'}
