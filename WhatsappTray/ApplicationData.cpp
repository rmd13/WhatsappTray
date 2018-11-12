/*
*
* WhatsappTray
* Copyright (C) 1998-2017  Sebastian Amann, Nikolay Redko, J.D. Purcell
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*/
#include "stdafx.h"

#include "ApplicationData.h"
#include "Logger.h"

#include <Shlobj.h>
#include <sstream>
#include <tchar.h>
#include <string>

#undef MODULE_NAME
#define MODULE_NAME "ApplicationData::"

#pragma WARNING(NEEDS CLEANUP)
ApplicationData::ApplicationData()
	: closeToTray(false, Data::CLOSE_TO_TRAY)
	, launchOnWindowsStartup(false, Data::LAUNCH_ON_WINDOWS_STARTUP)
{
	closeToTray.Value = GetDataOrSetDefault(closeToTray.Info.toString(), closeToTray.DefaultValue);
	launchOnWindowsStartup.Value = GetDataOrSetDefault(launchOnWindowsStartup.Info.toString(), launchOnWindowsStartup.DefaultValue);
}

/*
* Set the data in the persistant storage.
*/
bool ApplicationData::SetData(std::string key, bool value)
{
	// Write data to the settingsfile.
	std::string applicationDataFilePath = GetSettingsFile();
	if (WritePrivateProfileString("config", key.c_str(), value ? "1" : "0", applicationDataFilePath.c_str()) == NULL) {
		// We get here also when the folder does not exist.
		std::stringstream message;
		message << MODULE_NAME "SetData() - Saving application-data failed because the data could not be written in the settings file '" << applicationDataFilePath.c_str() << "'.";
		Logger::Error(message.str().c_str());
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return false;
	}
	return true;
}

bool ApplicationData::GetCloseToTray()
{
	return closeToTray.Value;
}

void ApplicationData::SetCloseToTray(bool value)
{
	closeToTray.Value = value;
	// Write data to persistant storage.
	SetData("CLOSE_TO_TRAY", value);
}

bool ApplicationData::GetLaunchOnWindowsStartup()
{
	return launchOnWindowsStartup.Value;
}

void ApplicationData::SetLaunchOnWindowsStartup(bool value)
{
	launchOnWindowsStartup.Value = value;
	SetData("LAUNCH_ON_WINDOWS_STARTUP", value);
}

bool ApplicationData::GetDataOrSetDefault(std::string key, bool defaultValue)
{
	std::string applicationDataFilePath = GetSettingsFile();

	TCHAR valueBuffer[200] = { 0 };
	if (GetPrivateProfileString("config", key.c_str(), defaultValue ? "1" : "0", valueBuffer, sizeof(valueBuffer), applicationDataFilePath.c_str()) == NULL) {
		std::stringstream message;
		message << MODULE_NAME "GetDataOrSetDefault() - Error while trying to read the settings file '" << applicationDataFilePath.c_str() << "'.";
		Logger::Error(message.str().c_str());
		MessageBox(NULL, message.str().c_str(), "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return defaultValue;
	}

	// Convert the value from a c-string to bool. "1" -> true otherwise false.
	bool value = _tcsncmp(TEXT("1"), valueBuffer, 1) == 0;
	return value;
}

std::string ApplicationData::GetSettingsFile()
{
	TCHAR applicationDataDirectoryPath[MAX_PATH] = { 0 };
	if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, applicationDataDirectoryPath) != S_OK) {
		Logger::Error(MODULE_NAME "GetSettingsFile() - Saving application-data failed because the path could not be received.");
		MessageBox(NULL, MODULE_NAME "GetSettingsFile() - Saving application-data failed because the path could not be received.", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return "";
	}

	// Create settings-filder if not exists.
	_tcscat_s(applicationDataDirectoryPath, MAX_PATH, TEXT("\\WhatsappTray"));
	if (CreateDirectory(applicationDataDirectoryPath, NULL) == NULL) {
		// We can get here, if the folder already exists -> We don't want an error for that case ...
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			Logger::Error(MODULE_NAME "GetSettingsFile() - Saving application-data failed because the folder could not be created.");
			MessageBox(NULL, MODULE_NAME "GetSettingsFile() - Saving application-data failed because the folder could not be created.", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
			return "";
		}
	}
	std::string applicationDataFilePath(applicationDataDirectoryPath);
	applicationDataFilePath.append("\\config.ini");

	return applicationDataFilePath;
}
