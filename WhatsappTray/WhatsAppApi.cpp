/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright(C) 1998 - 2018 WhatsappTray Sebastian Amann */

#include "stdafx.h"

#include "WhatsAppApi.h"
#include "WhatsappTray.h"

#include "DirectoryWatcher.h"
#include "AppData.h"
#include "Helper.h"
#include "Logger.h"

#include <regex>
#include <Mmsystem.h>
#include <Shlobj.h>
#include <filesystem>

namespace fs = std::experimental::filesystem;

#undef MODULE_NAME
#define MODULE_NAME "WhatsAppApi::"

/* Public */
bool WhatsAppApi::IsFullInit = false;

/* Private */
std::unique_ptr<DirectoryWatcher> WhatsAppApi::dirWatcher = std::unique_ptr<DirectoryWatcher>(nullptr);

std::function<void()> WhatsAppApi::receivedMessageEvent = NULL;
std::function<void()> WhatsAppApi::receivedFullInitEvent = NULL;

/**
 * @brief Initialize the class.
*/
void WhatsAppApi::Init()
{
	std::wstring leveldbDirectory = Helper::Utf8ToWide(std::string(AppData::WhatsappRoamingDirectoryGet()));

	// Add a slash to the end of the path if ther is none.
	auto lastCharacter = leveldbDirectory[leveldbDirectory.length() - 1];
	if (lastCharacter != '\\') {
		leveldbDirectory.append(L"\\");
	}

	leveldbDirectory.append(L"WhatsApp\\IndexedDB\\file__0.indexeddb.leveldb\\");

	fs::path leveldbDirectoryPath(leveldbDirectory);
	if (leveldbDirectoryPath.is_relative()) {
		fs::path appPath = Helper::GetApplicationFilePath();
		auto combinedPath = appPath / leveldbDirectoryPath;
		Logger::Info(MODULE_NAME "Init() - Setting leveldb-directory-path to combinedPath:%s", Helper::WideToUtf8(combinedPath.wstring()).c_str());

		// Shorten the path by converting to absoltue path.
		auto combinedPathCanonical = fs::canonical(combinedPath);
		leveldbDirectory = combinedPathCanonical.wstring();
	}

	Logger::Info(MODULE_NAME "Init() - Using leveldb-directory:%s", Helper::WideToUtf8(leveldbDirectory).c_str());

	if (fs::exists(fs::path(leveldbDirectory)) == false) {
		Logger::Fatal(MODULE_NAME "Init() - Could not get the leveldb-directory!");
		MessageBoxA(NULL, MODULE_NAME "Init() - Fatal: Could not get the leveldb-directory!", "WhatsappTray", MB_OK | MB_ICONINFORMATION);
		return;
	}
	WhatsAppApi::dirWatcher = std::unique_ptr<DirectoryWatcher>(new DirectoryWatcher(leveldbDirectory, &WhatsAppApi::IndexedDbChanged));
}

void WhatsAppApi::NotifyOnNewMessage(const std::function<void()>& receivedMessageHandler)
{
	receivedMessageEvent = receivedMessageHandler;
}

void WhatsAppApi::NotifyOnFullInit(const std::function<void()>& receivedStatusV3Handler)
{
	receivedFullInitEvent = receivedStatusV3Handler;
}

/**
 * NOTE: I noticed false positives when Whatsapp is started an not yet fully initialized.
 * NOTE: It is better to use wstring for paths because of unicode-charcters that can happen in other languages
 */
void WhatsAppApi::IndexedDbChanged(const DWORD dwAction, std::wstring strFilename)
{
	// The logfiles change so we have to keep track of them.
	static std::wstring lastLogfile = L"";
	static size_t processedLineCount = 0;

	if (strFilename.find(L".log") == std::string::npos) {
		return;
	}

	std::string lineBuffer;
	if (strFilename.compare(lastLogfile) != 0) {
		Logger::Info(MODULE_NAME "IndexedDbChanged() - The logfile has changed");
		lastLogfile = strFilename;

		// We have to reset the lineNumber when we find a new log-file ...
		// Open the file in binary-mode else the eof is wrong.
		std::ifstream file(strFilename.c_str(), std::ios_base::in | std::ios_base::binary);
		if (file.is_open() == false) {
			Logger::Error(MODULE_NAME "IndexedDbChanged() - file.is_open() == false");
			return;
		}

		for (size_t lineCounter = 0; true; ) {
			int character = file.get();

			if (character == '\r') {
				lineCounter++;
			}

			if (file.fail()) {
				Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.fail()");
				processedLineCount = lineCounter;
				break;
			}

			if (file.eof()) {
				Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.eof()");
				processedLineCount = lineCounter;
				break;
			}
		}

		return;
	}

	// Open the file in binary-mode else the eof is wrong.
	std::ifstream file(strFilename.c_str(), std::ios_base::in | std::ios_base::binary);
	if (file.is_open() == false) {
		Logger::Error(MODULE_NAME "IndexedDbChanged() - file.is_open() == false");
		return;
	}

	for (size_t lineCounter = 0; true; lineCounter++) {
		std::getline(file, lineBuffer, '\r');

		if (file.fail()) {
			Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.fail()");
			return;
		}

		if (file.eof()) {
			//Logger::Debug(MODULE_NAME "IndexedDbChanged() - file.eof()");
			return;
		}

		if (lineCounter <= processedLineCount) {
			// This line was already processed.
			continue;
		}
		processedLineCount = lineCounter;

		if (std::regex_search(lineBuffer.c_str(), std::regex("models:mute:cache"))) {
			Logger::Debug(MODULE_NAME "IndexedDbChanged() - Found match for fullInit.");

			IsFullInit = true;
			if (receivedFullInitEvent) {
				receivedFullInitEvent();
			}
		}

		if (lineBuffer.find("recv:") == std::string::npos) {
			continue;
		}

		//Logger::Debug(MODULE_NAME "IndexedDbChanged() - Found recv: '%s'", lineBuffer.c_str());

		// Match: recv: [0-9a-f]{16}[.]--[0-9a-f]+\"\ttimestampN
		if (std::regex_search(lineBuffer.c_str(), std::regex("recv: [0-9a-f]{16}[.]--[0-9a-f]+\"\\ttimestampN"))) {
			//Logger::Info(MODULE_NAME "IndexedDbChanged() - Found match for receivedMessage.");
			
			if (WhatsAppApi::IsFullInit && receivedMessageEvent) {
				receivedMessageEvent();
			}
		}

	}
}
