#ifndef __VERSION_H__
#define __VERSION_H__

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

// optional
#define VERSION_TAG "beta.2"
#define VERSION_NAME "\"Isa\""

const char *getversionstring(const bool name)
{
    static string version;
    formatstring(version, "v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
#ifdef VERSION_TAG
    concformatstring(version, "-%s", VERSION_TAG);
#endif
#ifdef VERSION_NAME
    if(name) concformatstring(version, " (%s)", VERSION_NAME);
#endif
    return version;
}

SVARRO(currentversion, getversionstring(false));
SVARRO(currentversionname, getversionstring(true));

#endif