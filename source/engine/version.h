#ifndef __VERSION_H__
#define __VERSION_H__

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_EDITION "Ymir"

VAR(currentversion, 1, VERSION_MAJOR << 16 | VERSION_MINOR << 8 | VERSION_PATCH, 0);
SVARRO(currentversionstring, tempformatstring("v%d.%d.%d (%s Edition)", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_EDITION));

const char *formatversion(int version)
{
    static string s;
    formatstring(s, "v%d.%d.%d", version >> 16, version >> 8 & 0xFF, version & 0xFF);
    return s;
}
ICOMMAND(formatversion, "i", (int *version), result(formatversion(*version)));

#endif