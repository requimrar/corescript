// FileReader.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <fstream>
#include <unordered_map>

#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "parser.h"
#include "compiler.h"

#ifdef __MACH__
#include <mach/vm_statistics.h>
#define EXTRA_MMAP_FLAGS VM_FLAGS_SUPERPAGE_SIZE_2MB
#elif defined(MAP_HUGE_2MB)
#define EXTRA_MMAP_FLAGS MAP_HUGE_2MB
#else
#define EXTRA_MMAP_FLAGS 0
#endif

using string_view = std::experimental::string_view;


namespace Lexer
{
	Parser::Token getNextToken(std::vector<string_view>& lines, size_t* line, const string_view& whole, Parser::Pin& pos);
}

namespace Compiler
{
	struct FileInnards
	{
		Parser::TokenList tokens;
		std::vector<string_view> lines;
		const char* contents;
		size_t contentLength = 0;

		bool isLexing = false;
		bool didLex = false;
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static void readFile(std::string fullPath)
	{
		using namespace Parser;

		char* fileContents = 0;
		size_t fileLength = 0;

		{
			auto p = prof::Profile("read file");

			// first, get the size of the file
			struct stat st;
			int ret = stat(fullPath.c_str(), &st);

			if(ret != 0)
			{
				perror("There was an error getting the file size");
				exit(-1);
			}

			fileLength = st.st_size;

			int fd = open(fullPath.c_str(), O_RDONLY);
			if(fd == -1)
			{
				perror("There was an error getting opening the file");
				exit(-1);
			}

			// check if we should mmap
			// explanation: if we have EXTRA_MMAP_FLAGS, then we're getting 2MB pages -- in which case we should probably only do it
			// if we have at least 4mb worth of file.
			// if not, then just 2 * pagesize.
			#define MINIMUM_MMAP_THRESHOLD (EXTRA_MMAP_FLAGS ? (2 * 2 * 1024 * 1024) : 2 * getpagesize())

			if(fileLength >= MINIMUM_MMAP_THRESHOLD)
			{
				// ok, do an mmap
				fileContents = (char*) mmap(0, fileLength, PROT_READ, MAP_PRIVATE | EXTRA_MMAP_FLAGS, fd, 0);
				if(fileContents == MAP_FAILED)
				{
					perror("There was an error getting reading the file");
					exit(-1);
				}
			}
			else
			{
				// read normally
				fileContents = new char[fileLength + 1];
				read(fd, fileContents, fileLength);
			}
			close(fd);
		}





		// std::ifstream fstr(fullPath);
		// std::string fileContents;
		// if(fstr)
		// {
		// 	auto p = prof::Profile("read file");

		// 	std::ostringstream contents;
		// 	contents << fstr.rdbuf();
		// 	fstr.close();
		// 	fileContents = contents.str();
		// }
		// else
		// {
		// 	perror("There was an error reading the file");
		// 	exit(-1);
		// }







		// split into lines
		std::vector<string_view> rawlines;

		{
			auto p = prof::Profile("lines");
			string_view view(fileContents, fileLength);

			while(true)
			{
				size_t ln = view.find('\n');
				if(ln != string_view::npos)
				{
					rawlines.push_back(view.substr(0, ln + 1));
					view.remove_prefix(ln + 1);
				}
				else
				{
					break;
				}
			}

			p.finish();
		}


		Pin pos;
		FileInnards& innards = fileList[fullPath];
		{
			pos.fileID = getFileIDFromFilename(fullPath);

			innards.lines = std::move(rawlines);
			innards.contents = fileContents;
			innards.isLexing = true;
		}


		auto p = prof::Profile("lex");
		TokenList& ts = innards.tokens;

		{
			// copy lines.
			auto lines = innards.lines;

			size_t curLine = 0;
			Token curtok;

			auto view = string_view(innards.contents);
			while((curtok = Lexer::getNextToken(lines, &curLine, view, pos)).type != TType::EndOfFile)
				ts.push_back(std::move(curtok));
		}

		p.finish();
		// prof::printResults();
		// exit(0);


		innards.didLex = true;
		innards.isLexing = false;



		/*
			~175ms reading with c++
			~20ms with read() -- split lines ~70ms
			~4ms with mmap() -- split lines ~87ms
		*/
	}


	Parser::TokenList& getFileTokens(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end() || !fileList[fullPath].didLex)
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}
		else if(fileList[fullPath].isLexing)
		{
			error("Cannot get token list of file '%s' while still lexing", fullPath.c_str());
		}

		return fileList[fullPath].tokens;
	}

	std::string getFileContents(std::string fullPath)
	{
		if(fileList.find(fullPath) == fileList.end())
		{
			readFile(fullPath);
			assert(fileList.find(fullPath) != fileList.end());
		}

		const auto& in = fileList[fullPath];
		return std::string(in.contents, in.contentLength);
	}


	static std::vector<std::string> fileNames { "null" };
	static std::unordered_map<std::string, size_t> existingNames;
	const std::string& getFilenameFromID(size_t fileID)
	{
		iceAssert(fileID > 0 && fileID < fileNames.size());
		return fileNames[fileID];
	}

	size_t getFileIDFromFilename(const std::string& name)
	{
		size_t i = existingNames[name];
		if(i == 0)
		{
			fileNames.push_back(name);
			return fileNames.size();
		}

		return i;
	}

	const std::vector<string_view>& getFileLines(size_t id)
	{
		std::string fp = getFilenameFromID(id);
		if(fileList.find(fp) == fileList.end())
		{
			readFile(fp);
			assert(fileList.find(fp) != fileList.end());
		}

		return fileList[fp].lines;
	}
}









