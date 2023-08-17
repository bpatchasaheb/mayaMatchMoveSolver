@ECHO OFF
SETLOCAL
::
:: Copyright (C) 2019 David Cattermole.
::
:: This file is part of mmSolver.
::
:: mmSolver is free software: you can redistribute it and/or modify it
:: under the terms of the GNU Lesser General Public License as
:: published by the Free Software Foundation, either version 3 of the
:: License, or (at your option) any later version.
::
:: mmSolver is distributed in the hope that it will be useful,
:: but WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
:: GNU Lesser General Public License for more details.
::
:: You should have received a copy of the GNU Lesser General Public License
:: along with mmSolver.  If not, see <https://www.gnu.org/licenses/>.
:: ---------------------------------------------------------------------
::
:: Build the mmSolver plug-in.
::
:: NOTE: Do not call this script directly! This file should be called by
:: the build_mmSolver_windows64_mayaXXXX.bat files.
::
:: This file assumes the MAYA_VERSION and MAYA_LOCATION variables
:: have been set.

:: The root of this project.
SET PROJECT_ROOT=%CD%
ECHO Project Root: %PROJECT_ROOT%

:: What directory to build the project in?
SET BUILD_DIR_BASE=%PROJECT_ROOT%\..

:: Run the Python API and Solver tests inside Maya, after a
:: successfully build an install process.
SET RUN_TESTS=0

:: Where to install the module?
::
:: Note: In Windows 8 and 10, "My Documents" is no longer visible,
::       however files copying to "My Documents" automatically go
::       to the "Documents" directory.
::
:: The "$HOME/maya/2018/modules" directory is automatically searched
:: for Maya module (.mod) files. Therefore we can install directly.
::
:: SET INSTALL_MODULE_DIR="%PROJECT_ROOT%\modules"
SET INSTALL_MODULE_DIR="%USERPROFILE%\My Documents\maya\%MAYA_VERSION%\modules"

:: Build ZIP Package.
:: For developer use. Make ZIP packages ready to distribute to others.
SET BUILD_PACKAGE=1

:: Do not edit below, unless you know what you're doing.
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:: What type of build? "Release" or "Debug"?
SET BUILD_TYPE=Release

:: Build options, to allow faster compilation times. (not to be used by
:: users wanting to build this project.)
SET MMSOLVER_BUILD_PLUGIN=1
SET MMSOLVER_BUILD_TOOLS=1
SET MMSOLVER_BUILD_PYTHON=1
SET MMSOLVER_BUILD_MEL=1
SET MMSOLVER_BUILD_3DEQUALIZER=1
SET MMSOLVER_BUILD_SYNTHEYES=1
SET MMSOLVER_BUILD_BLENDER=1
SET MMSOLVER_BUILD_QT_UI=1
SET MMSOLVER_BUILD_RENDERER=1
SET MMSOLVER_BUILD_DOCS=1
SET MMSOLVER_BUILD_ICONS=1
SET MMSOLVER_BUILD_CONFIG=1
SET MMSOLVER_BUILD_TESTS=0

SET PYTHON_VIRTUAL_ENV_DIR_NAME=python_venv_windows64_maya%MAYA_VERSION%

:: Note: There is no need to deactivate the virtual environment because
:: this batch script is 'SETLOCAL' (see top of file) and therefore no
:: environment variables are leaked into the calling environment.
CALL %PROJECT_ROOT%\scripts\internal\python_venv_activate.bat

:: Paths for mmSolver library dependencies.
SET MMSOLVERLIBS_INSTALL_DIR="%BUILD_DIR_BASE%\build_mmsolverlibs\install\maya%MAYA_VERSION%_windows64"
SET MMSOLVERLIBS_CMAKE_CONFIG_DIR="%MMSOLVERLIBS_INSTALL_DIR%\lib\cmake\mmsolverlibs"

:: MinGW is a common install for developers on Windows and
:: if installed and used it will cause build conflicts and
:: errors, so we disable it.
SET IGNORE_INCLUDE_DIRECTORIES=""
IF EXIST "C:\MinGW" (
    SET IGNORE_INCLUDE_DIRECTORIES="C:\MinGW\bin;C:\MinGW\include"
)

:: Optionally use "NMake Makefiles" as the build system generator.
SET CMAKE_GENERATOR=Ninja

:: Force the compilier to be MSVC's cl.exe, so that if other
:: compiliers are installed, CMake doesn't get confused and try to use
:: it (such as clang).
SET CMAKE_C_COMPILER=cl
SET CMAKE_CXX_COMPILER=cl

:: Build project
SET BUILD_DIR_NAME=cmake_win64_maya%MAYA_VERSION%_%BUILD_TYPE%
SET BUILD_DIR=%BUILD_DIR_BASE%\build_mmsolver\%BUILD_DIR_NAME%
ECHO BUILD_DIR_BASE: %BUILD_DIR_BASE%
ECHO BUILD_DIR_NAME: %BUILD_DIR_NAME%
ECHO BUILD_DIR: %BUILD_DIR%
CHDIR "%BUILD_DIR_BASE%"
MKDIR "build_mmsolver"
CHDIR "%BUILD_DIR_BASE%\build_mmsolver"
MKDIR "%BUILD_DIR_NAME%"
CHDIR "%BUILD_DIR%"

%CMAKE_EXE% -G %CMAKE_GENERATOR% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX=%INSTALL_MODULE_DIR% ^
    -DCMAKE_IGNORE_PATH=%IGNORE_INCLUDE_DIRECTORIES% ^
    -DCMAKE_CXX_STANDARD=%CXX_STANDARD% ^
    -DCMAKE_C_COMPILER=%CMAKE_C_COMPILER% ^
    -DCMAKE_CXX_COMPILER=%CMAKE_CXX_COMPILER% ^
    -DMMSOLVER_BUILD_PLUGIN=%MMSOLVER_BUILD_PLUGIN% ^
    -DMMSOLVER_BUILD_TOOLS=%MMSOLVER_BUILD_TOOLS% ^
    -DMMSOLVER_BUILD_PYTHON=%MMSOLVER_BUILD_PYTHON% ^
    -DMMSOLVER_BUILD_MEL=%MMSOLVER_BUILD_MEL% ^
    -DMMSOLVER_BUILD_3DEQUALIZER=%MMSOLVER_BUILD_3DEQUALIZER% ^
    -DMMSOLVER_BUILD_SYNTHEYES=%MMSOLVER_BUILD_SYNTHEYES% ^
    -DMMSOLVER_BUILD_BLENDER=%MMSOLVER_BUILD_BLENDER% ^
    -DMMSOLVER_BUILD_QT_UI=%MMSOLVER_BUILD_QT_UI% ^
    -DMMSOLVER_BUILD_RENDERER=%MMSOLVER_BUILD_RENDERER% ^
    -DMMSOLVER_BUILD_DOCS=%MMSOLVER_BUILD_DOCS% ^
    -DMMSOLVER_BUILD_ICONS=%MMSOLVER_BUILD_ICONS% ^
    -DMMSOLVER_BUILD_CONFIG=%MMSOLVER_BUILD_CONFIG% ^
    -DMMSOLVER_BUILD_TESTS=%MMSOLVER_BUILD_TESTS% ^
    -DMAYA_LOCATION=%MAYA_LOCATION% ^
    -DMAYA_VERSION=%MAYA_VERSION% ^
    -Dmmsolverlibs_DIR=%MMSOLVERLIBS_CMAKE_CONFIG_DIR% ^
    %PROJECT_ROOT%
if errorlevel 1 goto failed_to_generate

%CMAKE_EXE% --build . --parallel
if errorlevel 1 goto failed_to_build

:: Comment this line out to stop the automatic install into the home directory.
%CMAKE_EXE% --install .
if errorlevel 1 goto failed_to_install

:: Run tests
IF "%RUN_TESTS%"=="1" (
    %CMAKE_EXE% --build . --target test
)

:: Create a .zip package.
IF "%BUILD_PACKAGE%"=="1" (
    %CMAKE_EXE% --build . --target package
    if errorlevel 1 goto failed_to_build_zip
)

:: Return back project root directory.
CHDIR "%PROJECT_ROOT%"
exit /b 0

:failed_to_generate
echo Failed to generate build files.
exit /b 1

:failed_to_build
echo Failed to build.
exit /b 1

:failed_to_install
echo Failed to install.
exit /b 1

:failed_to_build_zip
echo Failed to build the ZIP package file.
exit /b 1
