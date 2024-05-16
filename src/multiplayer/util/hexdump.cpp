#include <iostream>
#include <iomanip>
#include <sstream>

// Source: https://codereview.stackexchange.com/questions/165120/printing-hex-dumps-for-diagnostics

constexpr size_t WIDTH = 16;

std::ostream& HexDump(std::ostream& os, const void *buffer,
		std::size_t bufsize, bool showPrintableChars = true) {
	if (buffer == nullptr) {
		return os;
	}

	auto oldFormat = os.flags();
	auto oldFillChar = os.fill();
	constexpr std::size_t maxline{ WIDTH };
	// create a place to store text version of string
	char renderString[maxline+1];
	char *rsptr{renderString};
	// convenience cast
	const unsigned char *buf{reinterpret_cast<const unsigned char *>(buffer)};

	for (std::size_t linecount=maxline; bufsize; --bufsize, ++buf) {
		os << std::setw(2) << std::setfill('0') << std::hex
			<< static_cast<unsigned>(*buf) << ' ';
		*rsptr++ = std::isprint(*buf) ? *buf : '.';
		if (--linecount == 0) {
			*rsptr++ = '\0'; // terminate string
			if (showPrintableChars) {
				os << "| " << renderString;
			}
			os << '\n';
			rsptr = renderString;
			linecount = std::min(maxline, bufsize);
		}
	}
	// emit newline if we haven't already
	if (rsptr != renderString) {
		if (showPrintableChars) {
			for (*rsptr++ = '\0'; rsptr != &renderString[maxline+1]; ++rsptr) {
				os << "   ";
			}
			os << "| " << renderString;
		}
		os << '\n';
	}

	os.fill(oldFillChar);
	os.flags(oldFormat);
	return os;
}

std::string HexDump(const void *buf, std::size_t bufsz) {
	std::ostringstream oss;
	HexDump(oss, buf, bufsz);
	return oss.str();
}

std::string HexDump(std::string_view data) {
	return HexDump(data.data(), data.size());
}
