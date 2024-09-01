// test file used to check if `libmaxminddb` is installed
#include <maxminddb.h>

int main()
{
    MMDB_s *mmdb = NULL;
    MMDB_close(mmdb);
    return 0;
}