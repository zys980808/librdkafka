@echo off

SET TOOLCHAIN=v140

FOR %%C IN (Debug,Release) DO (
  FOR %%P IN (Win32,x64) DO (
     @echo Building %%C %%P
     msbuild librdkafka.sln /p:Configuration=%%C /p:Platform=%%P /target:Clean
     msbuild librdkafka.sln /p:Configuration=%%C /p:Platform=%%P || goto :error

     REM lib /OUT:outdir\%TOOLCHAIN%\%%P\%%C\librdkafka-static.lib interim\%TOOLCHAIN%/%%P\%%C\*.obj
     REM @echo "Static C library in outdir\%TOOLCHAIN%\%%P\%%C\librdkafka-static.lib"

     REM lib /OUT:outdir\%TOOLCHAIN%\%%P\%%C\librdkafkacpp-static.lib librdkafkacpp\interim\%TOOLCHAIN%/%%P\%%C\*.obj
     REM @echo "Static C++ library in outdir\%TOOLCHAIN%\%%P\%%C\librdkafkacpp-static.lib"
  )
)

exit /b 0

:error
echo "Build failed"
exit /b 1
