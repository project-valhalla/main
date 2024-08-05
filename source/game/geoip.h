#ifdef STANDALONE
#include <maxminddb.h>

MMDB_s *mmdb = NULL;
#endif

string _geoip_filename;

void geoip_close_database()
{
    #ifdef STANDALONE
    if(!mmdb) return;
    MMDB_close(mmdb);
    free(mmdb);
    mmdb = NULL;
    #endif
}

void geoip_open_database(int geoip)
{
    #ifdef STANDALONE
    if(mmdb) geoip_close_database();
    if(!geoip || !_geoip_filename[0]) return;

    mmdb = (MMDB_s *)calloc(1, sizeof(MMDB_s));
    if(!mmdb) return;

    int status = MMDB_open(_geoip_filename, MMDB_MODE_MMAP, mmdb);
    if(MMDB_SUCCESS != status)
    {
        free(mmdb);
        mmdb = NULL;
    }
    #endif
}

void geoip_lookup_ip(enet_uint32 ip, char *country_code, char *country_name)
{
    #ifdef STANDALONE
    if(!mmdb) return;
    int error;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, &ip, sizeof(enet_uint32));

    MMDB_lookup_result_s result = MMDB_lookup_sockaddr(mmdb, (struct sockaddr *)&sin, &error);
    if(MMDB_SUCCESS == error && result.found_entry)
    {
        MMDB_entry_data_s data;

        // get country code
        error = MMDB_get_value(&result.entry, &data, "country", "iso_code", NULL);
        if(MMDB_SUCCESS == error && data.has_data && MMDB_DATA_TYPE_UTF8_STRING == data.type && data.data_size >= 2)
        {
            copystring(country_code, data.utf8_string, data.data_size+1);
        }

        // get country name
        error = MMDB_get_value(&result.entry, &data, "country", "names", "en", NULL);
        if(MMDB_SUCCESS == error && data.has_data && MMDB_DATA_TYPE_UTF8_STRING == data.type)
        {
            uchar buf[MAXSTRLEN];
            size_t len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)data.utf8_string, data.data_size);
            if(len > 0) {
                buf[len] = 0;
                copystring(country_name, (const char*)buf, len+1);
            }
        }
    }
    else
    {
        country_code[0] = '\0';
        country_name[0] = '\0';
    }

    #else
    country_code[0] = '\0';
    country_name[0] = '\0';
    #endif
}

VARF(geoip, 0, 0, 1, geoip_open_database(geoip));
SVARF(geoip_database, "geoip.mmdb", {
    #ifdef STANDALONE
    copystring(_geoip_filename, geoip_database, MAXSTRLEN);
    #endif
    geoip_open_database(geoip);
});